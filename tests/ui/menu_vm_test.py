#!/usr/bin/env python3
"""Compare a menu refactor against a saved baseline EFI binary in isolated VMs."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import struct
import subprocess
import time
from collections.abc import Generator, Sequence
from contextlib import contextmanager
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
OVMF_CODE = os.environ.get("OVMF_CODE", "/usr/share/edk2/x64/OVMF_CODE.4m.fd")
OVMF_VARS = os.environ.get("OVMF_VARS", "/usr/share/edk2/x64/OVMF_VARS.4m.fd")
WORK = ROOT / "work/menu-refactor"
HEADER = "timeout: no\ninterface_resolution: 1280x720\nwallpaper: boot():/boot/bg.jpg\n"
ENTRIES = """
/Boot A
    comment: First payload
    protocol: limine
    path: boot():/boot/test.elf
    cmdline: fixture-a
/Group
    //Child
        protocol: limine
        path: boot():/boot/test.elf
        cmdline: fixture-child
    //Nested
        ///Grandchild
            protocol: limine
            path: boot():/boot/test.elf
            cmdline: fixture-grandchild
/Hidden BIOS entry
    protocol: bios
/Boot Z
    comment: Last payload
    protocol: limine
    path: boot():/boot/test.elf
    cmdline: fixture-z
"""


def command(*args: str) -> None:
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


class VM:
    def __init__(self, directory: Path, binary: Path, config: str | None,
                 extra_files: Sequence[tuple[Path, str]] = (), extra_qemu: Sequence[str] = (),
                 bios: Path | None = None, video: Sequence[str] = ("-vga", "std"),
                 memory: int = 512, machine: str = "q35",
                 ui_config: str | None = None, config_directory: str = "/") -> None:
        self.directory = directory
        directory.mkdir(parents=True, exist_ok=True)
        self.sock = directory / "qmp.sock"
        self.sock.unlink(missing_ok=True)
        self.disk = directory / "esp.img"
        with self.disk.open("wb") as f:
            f.truncate(128 * 1024 * 1024)
            if bios is not None:
                f.seek(446)
                f.write(struct.pack("<B3sB3sII", 0x80, b"\xfe\xff\xff", 0x0c,
                                    b"\xfe\xff\xff", 2048, 128 * 2048 - 2048))
                f.seek(510)
                f.write(b"\x55\xaa")
        fat = str(self.disk) + ("@@1048576" if bios is not None else "")
        command("mformat", "-i", fat, "-F", "::")
        command("mmd", "-i", fat, "::/EFI", "::/EFI/BOOT", "::/boot")
        files = [(binary, "::/EFI/BOOT/BOOTX64.EFI"),
                 (ROOT / "test/test.elf", "::/boot/test.elf"),
                 (ROOT / "test/bg.jpg", "::/boot/bg.jpg")]
        files.extend(extra_files)
        if bios is not None:
            files.append((bios / "limine-bios.sys", "::/boot/limine-bios.sys"))
        config_directory = "/" + config_directory.strip("/")
        config_directory = config_directory.rstrip("/") + "/"
        if ui_config is not None:
            (directory / "eucleia.conf").write_text(ui_config)
            files.append((directory / "eucleia.conf", "::" + config_directory + "eucleia.conf"))
        if config is not None:
            (directory / "limine.conf").write_text(config)
            files.append((directory / "limine.conf", "::" + config_directory + "limine.conf"))
        directories = {"/", "/efi", "/efi/boot", "/boot"}
        for source, target in files:
            for parent in reversed(Path(target.removeprefix("::")).parents):
                if str(parent).lower() not in directories:
                    command("mmd", "-i", fat, "::" + str(parent))
                    directories.add(str(parent).lower())
            command("mcopy", "-i", fat, str(source), target)
        firmware = []
        if bios is None:
            shutil.copyfile(OVMF_VARS, directory / "vars.fd")
            firmware = [
                "-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={OVMF_CODE}",
                "-drive", f"if=pflash,format=raw,unit=1,file={directory / 'vars.fd'}"]
        else:
            command(str(bios / "limine"), "bios-install", str(self.disk))
        args = [
            "qemu-system-x86_64", "-machine", machine, "-accel", "kvm", "-cpu", "host",
            "-m", str(memory), "-smp", "2", "-nic", "none", "-display", "none", *video,
            *firmware,
            "-drive", f"format=raw,{('if=ide,snapshot=on' if bios is not None else 'if=virtio,readonly=on')},file={self.disk}",
            "-qmp", f"unix:{self.sock},server=on,wait=off", "-monitor", "none",
            "-serial", f"file:{directory / 'serial.log'}",
            "-debugcon", f"file:{directory / 'debugcon.log'}",
            "-global", "isa-debugcon.iobase=0xe9", *extra_qemu]
        (directory / "command.json").write_text(json.dumps(args, indent=2) + "\n")
        with (directory / "qemu.log").open("w") as log:
            self.process = subprocess.Popen(args, stdout=log, stderr=log)
        for _ in range(100):
            if self.process.poll() is not None:
                raise RuntimeError((directory / "qemu.log").read_text())
            try:
                self.qmp("query-status")
                break
            except (FileNotFoundError, ConnectionRefusedError):
                pass
            time.sleep(0.05)
        else:
            self.close()
            raise RuntimeError(f"QMP did not become ready: {directory}")

    def qmp(self, name: str, args: dict[str, object] | None = None) -> Any:
        with socket.socket(socket.AF_UNIX) as sock:
            sock.settimeout(5)
            sock.connect(str(self.sock))
            with sock.makefile("rwb") as f:
                def read() -> dict[str, Any]:
                    while True:
                        line = f.readline()
                        if not line:
                            raise EOFError(name)
                        value = json.loads(line)
                        if "event" not in value:
                            return value
                def send(command: str, arguments: dict[str, object]) -> Any:
                    f.write((json.dumps({"execute": command, "arguments": arguments}) + "\n").encode())
                    f.flush()
                    result = read()
                    if "error" in result:
                        raise RuntimeError(result)
                    return result.get("return")
                read()
                send("qmp_capabilities", {})
                return send(name, args or {})

    def capture(self, name: str) -> Path:
        path = self.directory / f"{name}.png"
        self.qmp("screendump", {"filename": str(path), "format": "png"})
        return path

    def menu_ready(self, fallback: bool = False, resolution: tuple[int, int] = (1280, 720)) -> None:
        if fallback:
            time.sleep(4)
            return
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            path = self.capture("waiting")
            with Image.open(path) as im:
                if im.size == resolution:
                    time.sleep(0.3)
                    return
            time.sleep(0.2)
        raise AssertionError(f"Menu did not appear: {self.directory}")

    def key(self, *keys: str) -> None:
        self.qmp("send-key", {"keys": [{"type": "qcode", "data": k} for k in keys], "hold-time": 40})
        time.sleep(0.12)

    def text(self, value: str) -> None:
        mapping = {":": ("shift", "semicolon"), " ": ("spc",), "-": ("minus",), "\n": ("ret",)}
        for char in value:
            self.key(*mapping.get(char, (char,)))

    def booted(self, marker: str) -> None:
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            log = (self.directory / "debugcon.log").read_text(errors="replace")
            if f"Command line: {marker}" in log and "We're alive" in log:
                return
            time.sleep(0.1)
        raise AssertionError(f"Missing boot marker {marker}: {self.directory}")

    def close(self) -> None:
        try:
            self.qmp("quit")
        except (OSError, EOFError):
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=5)
        self.disk.unlink(missing_ok=True)


@contextmanager
def guest(directory: Path, binary: Path, config: str | None,
          fallback: bool = False, ready: bool = True) -> Generator[VM, None, None]:
    vm = VM(directory, binary, config)
    try:
        if ready:
            vm.menu_ready(fallback)
        yield vm
    finally:
        vm.close()


def exercise(binary: Path, directory: Path) -> dict[str, Path]:
    screenshots: dict[str, Path] = {}
    with guest(directory / "navigation", binary, HEADER + ENTRIES) as vm:
        def snap(name: str) -> None:
            screenshots[name] = vm.capture(name)
        snap("initial")
        for key, name in [("up", "wrap-up"), ("down", "wrap-down"), ("end", "end"),
                          ("home", "home"), ("down", "group"), ("ret", "expanded"),
                          ("down", "child"), ("down", "nested"), ("right", "nested-expanded"),
                          ("ret", "nested-collapsed"), ("home", "back-home"),
                          ("e", "editor"), ("esc", "editor-cancelled"),
                          ("b", "blank-editor"), ("esc", "blank-cancelled")]:
            vm.key(key)
            snap(name)
        vm.key("e")
        vm.text("cmdline: edited-a\n")
        snap("editor-modified")
        vm.key("f10")
        vm.booted("edited-a")
    large = HEADER + "interface_branding: Long-list regression\n" + "".join(
        f"\n/Entry {i:02d} {'long-name-' * 16 if i == 39 else ''}\n"
        f" protocol: limine\n path: boot():/boot/test.elf\n cmdline: large-{i}\n" for i in range(40))
    with guest(directory / "scroll", binary, large) as vm:
        screenshots["scroll-top"] = vm.capture("scroll-top")
        vm.key("end")
        screenshots["scroll-bottom"] = vm.capture("scroll-bottom")
        vm.key("home")
        vm.key("3")
        vm.booted("large-2")
    with guest(directory / "countdown", binary, HEADER.replace("timeout: no", "timeout: 20.5") + ENTRIES) as vm:
        screenshots["countdown"] = vm.capture("countdown")
        vm.key("down")
        screenshots["countdown-cancelled"] = vm.capture("countdown-cancelled")
        vm.key("home")
        vm.key("ret")
        vm.booted("fixture-a")
    for name, options, marker in [
        ("autoboot", "timeout: 0.25\n", "fixture-a"),
        ("immediate", "timeout: 0\n", "fixture-a"),
        ("quiet", "quiet: yes\ntimeout: 0.25\n", "fixture-a"),
        ("default-path", "default_entry: Group/Nested/Grandchild\ntimeout: 0\n", "fixture-grandchild"),
    ]:
        with guest(directory / name, binary, options + HEADER.split("\n", 1)[1] + ENTRIES, ready=False) as vm:
            vm.booted(marker)
    for name, config in [
        ("empty", HEADER),
        ("no-config", None),
        ("disabled-help", HEADER + "editor_enabled: no\ninterface_help_hidden: yes\n" + ENTRIES),
        ("colours", HEADER + "interface_branding: Custom branding\ninterface_branding_colour: aa8855\ninterface_help_colour: 5577aa\n" + ENTRIES),
        ("invalid-default", "default_entry: 999\n" + HEADER.replace("timeout: no", "timeout: 0") + ENTRIES),
        ("directory-default", "default_entry: Group\n" + HEADER.replace("timeout: no", "timeout: 0") + ENTRIES),
        ("quiet-empty", "quiet: yes\n" + HEADER),
        ("fallback", HEADER + "graphics: no\n" + ENTRIES),
    ]:
        with guest(directory / name, binary, config, fallback=name in ("fallback", "no-config")) as vm:
            screenshots[name] = vm.capture(name)
            if name == "fallback":
                vm.key("ret")
                vm.booted("fixture-a")
    with guest(directory / "quiet-cancel", binary,
               "quiet: yes\n" + HEADER.replace("timeout: no", "timeout: 30") + ENTRIES,
               fallback=True) as vm:
        vm.key("down")
        vm.menu_ready()
        screenshots["quiet-cancelled"] = vm.capture("quiet-cancelled")
        vm.key("home")
        vm.key("ret")
        vm.booted("fixture-a")
    broken = HEADER + ENTRIES.replace("boot():/boot/test.elf", "boot():/missing.elf", 1)
    with guest(directory / "failed-boot", binary, broken) as vm:
        vm.key("ret")
        time.sleep(1)
        vm.key("spc")
        vm.menu_ready()
        screenshots["failed-boot-return"] = vm.capture("failed-boot-return")
        vm.key("e")
        vm.key("f10")
        time.sleep(1)
        vm.key("spc")
        vm.menu_ready()
        screenshots["failed-editor-return"] = vm.capture("failed-editor-return")
        vm.key("esc")
        screenshots["failed-editor-cancel"] = vm.capture("failed-editor-cancel")
        vm.key("end")
        vm.key("ret")
        vm.booted("fixture-z")
    return screenshots


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, default=ROOT / "work/build/bin/BOOTX64.EFI")
    args = parser.parse_args()
    images: dict[str, dict[str, Path]] = {}
    for label, binary in (("before", args.baseline), ("after", args.candidate)):
        print(f"Exercising {label}: {binary}", flush=True)
        images[label] = exercise(binary.resolve(), WORK / label)
    differences: list[str] = []
    expected_differences: list[str] = []
    for name, before in images["before"].items():
        with Image.open(before).convert("RGB") as a, Image.open(images["after"][name]).convert("RGB") as b:
            if a.size != b.size or ImageChops.difference(a, b).getbbox() is not None:
                if name == "quiet-empty":
                    # A complete frame now matches the normal empty-menu frame.
                    with Image.open(images["after"]["empty"]).convert("RGB") as normal:
                        if b.size == normal.size and ImageChops.difference(b, normal).getbbox() is None:
                            expected_differences.append(name)
                            continue
                differences.append(name)
    report = {"screenshots_compared": len(images["before"]), "differences": differences,
              "expected_differences": expected_differences,
              "baseline": str(args.baseline.resolve()), "candidate": str(args.candidate.resolve())}
    (WORK / "vm-results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)
    if differences:
        raise SystemExit("Framebuffer differences require inspection.")


if __name__ == "__main__":
    main()
