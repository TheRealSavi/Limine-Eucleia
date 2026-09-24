#!/usr/bin/env python3
"""Build and emulate Limine-Eucleia using project-local VM files."""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import shutil
import socket
import subprocess
import sys
import time
from collections.abc import Sequence
from pathlib import Path, PurePosixPath
from typing import Any

ROOT = Path(__file__).resolve().parent
WORK = ROOT / "work"
BUILD = WORK / "build"
VM = WORK / "vm"
LOGS = WORK / "logs"
QMP = VM / "qmp.sock"
CODE = Path(os.environ.get("OVMF_CODE", "/usr/share/edk2/x64/OVMF_CODE.4m.fd"))
VARS = Path(os.environ.get("OVMF_VARS", "/usr/share/edk2/x64/OVMF_VARS.4m.fd"))


def execute(args: Sequence[str], cwd: Path = ROOT, log: str | None = None) -> None:
    if log:
        with (LOGS / log).open("w") as stream:
            result = subprocess.run(args, cwd=cwd, stdout=stream,
                                    stderr=subprocess.STDOUT, check=False)
        if result.returncode:
            print((LOGS / log).read_text()[-8000:], file=sys.stderr)
            raise SystemExit(f"Command failed; see {LOGS / log}")
    else:
        subprocess.run(args, cwd=cwd, check=True)


def environment() -> dict[str, str]:
    env = os.environ.copy()
    libs = WORK / "tools/usr/lib"
    if (libs / "qemu/ui-gtk.so").exists():
        env["QEMU_MODULE_DIR"] = str(libs / "qemu")
        env["LD_LIBRARY_PATH"] = str(libs) + (
            ":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
    return env


def qmp(command: str, arguments: dict[str, object] | None = None) -> Any:
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(5)
        sock.connect(str(QMP))
        with sock.makefile("rwb") as stream:
            def receive() -> dict[str, Any]:
                while True:
                    line = stream.readline()
                    if not line:
                        raise EOFError("QEMU closed the QMP connection")
                    reply = json.loads(line)
                    if "event" not in reply:
                        return reply

            def send(name: str, args: dict[str, object] | None = None) -> Any:
                stream.write((json.dumps({"execute": name,
                                          "arguments": args or {}}) + "\n").encode())
                stream.flush()
                reply = receive()
                if "error" in reply:
                    raise RuntimeError(reply["error"]["desc"])
                return reply["return"]

            receive()
            send("qmp_capabilities")
            try:
                return send(command, arguments)
            except (ConnectionResetError, EOFError):
                if command != "quit":
                    raise
                return None


def status() -> Any:
    try:
        return qmp("query-status")
    except (FileNotFoundError, ConnectionRefusedError, ConnectionResetError, EOFError):
        return None


def check(graphical: bool = True) -> None:
    required = ("git", "make", "gcc", "clang", "ld.lld", "llvm-objcopy",
                "llvm-objdump", "llvm-readelf", "nasm", "autoreconf", "automake",
                "patch", "mformat", "mmd", "mcopy", "qemu-system-x86_64")
    missing = [name for name in required if not shutil.which(name)]
    missing.extend(str(path) for path in (CODE, VARS) if not path.is_file())
    if missing:
        raise SystemExit("Missing tools/firmware: " + ", ".join(missing))
    execute([sys.executable, "-c", "import PIL, fontTools"], log="python-dependencies.log")
    displays = subprocess.check_output(
        ["qemu-system-x86_64", "-display", "help"], env=environment(), text=True)
    if graphical and "gtk" not in displays.split():
        raise SystemExit("GTK display missing. Run: python tools/setup-display.py")
    print("Build tools and OVMF ready; accelerator: " + accelerator())
    print("QEMU display backends: " + ", ".join(
        x.strip() for x in displays.splitlines()[1:] if x.strip() and " " not in x.strip()))


def accelerator() -> str:
    return "kvm" if os.access("/dev/kvm", os.R_OK | os.W_OK) else "tcg"


def build() -> None:
    check(graphical=False)
    if not (ROOT / "configure").exists():
        print("Bootstrapping pinned upstream dependencies...", flush=True)
        execute(["./bootstrap"], log="bootstrap.log")
    if not (ROOT / "freetype/include/ft2build.h").exists():
        execute(["git", "clone", "--no-checkout", "https://github.com/freetype/freetype.git", "freetype"],
                log="freetype-fetch.log")
        execute(["git", "-C", "freetype", "checkout", "--detach",
                 "0a0221a1347e2f1e07c395263540026e9a0aa7c7"], log="freetype-checkout.log")
    if not (BUILD / "GNUmakefile").exists():
        execute([str(ROOT / "configure"), "--enable-uefi-x86-64",
                 "--disable-bios", "--disable-uefi-cd"], cwd=BUILD, log="configure.log")
    print("Building UEFI loader and test kernel...", flush=True)
    execute(["make", f"-j{min(os.cpu_count() or 2, 16)}"], cwd=BUILD, log="build.log")
    execute(["make", "-C", "test", "-f", "test.mk", "ARCH=x86",
             "CC_FOR_TARGET=clang", "LD_FOR_TARGET=ld.lld", "GREP=grep", "test.elf"],
            log="kernel-build.log")
    print(f"Built {BUILD / 'bin/BOOTX64.EFI'}")
    execute([sys.executable, "tools/build-theme.py"], log="theme-build.log")


def ui_assets(config: Path) -> dict[str, Path]:
    section = ""
    assets: dict[str, Path] = {}
    reserved = {"/limine.conf", "/eucleia.conf", "/boot/test.elf", "/boot/bg.jpg",
                "/efi/boot/bootx64.efi"}
    for line in config.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            continue
        key, colon, value = line.partition(":")
        key, value = key.strip(), value.strip()
        if not colon or not value:
            continue
        if not ((section == "" and key == "theme")
                or (section in {"background", "heading", "cursor"} and key == "file")
                or (section in ("heading", "menu.item.label", "hints", "details") and key == "font")
                or (section.startswith('entry "') and key == "icon")):
            continue
        relative = PurePosixPath(value.lstrip("/"))
        if ".." in relative.parts or str(relative) == ".":
            raise SystemExit("VM staging requires asset paths within the config directory: " + value)
        destination = "/" + str(relative)
        if destination.lower() in reserved:
            raise SystemExit("UI asset conflicts with a VM boot file: " + value)
        source = config.parent / str(relative)
        if destination == "/boot/eucleia.eui" and not source.is_file():
            source = WORK / "theme/eucleia.eui"
        if not source.is_file():
            print("UI asset unavailable; the guest will report it: " + str(source), file=sys.stderr)
            continue
        assets[destination] = source
    return assets


def stage(config: Path) -> None:
    assets = ui_assets(config)
    disk = VM / "esp.img"
    with disk.open("wb") as stream:
        stream.truncate(128 * 1024 * 1024)
    execute(["mformat", "-i", str(disk), "-F", "-v", "EUCLEIA", "::"])
    execute(["mmd", "-i", str(disk), "::/EFI", "::/EFI/BOOT", "::/boot"])
    for source, destination in (
        (BUILD / "bin/BOOTX64.EFI", "::/EFI/BOOT/BOOTX64.EFI"),
        (ROOT / "examples/limine.conf", "::/limine.conf"),
        (config, "::/eucleia.conf"),
        (ROOT / "test/test.elf", "::/boot/test.elf"),
        (ROOT / "test/bg.jpg", "::/boot/bg.jpg"),
        (WORK / "theme/eucleia.eui", "::/boot/eucleia.eui"),
    ):
        execute(["mcopy", "-i", str(disk), str(source), destination])
    directories = {"/", "/EFI", "/EFI/BOOT", "/boot"}
    for destination, source in assets.items():
        for parent in reversed(PurePosixPath(destination).parents):
            path = str(parent)
            if path not in directories:
                execute(["mmd", "-i", str(disk), "::" + path])
                directories.add(path)
        execute(["mcopy", "-o", "-i", str(disk), str(source), "::" + destination])
    if not (VM / "OVMF_VARS.fd").exists():
        shutil.copyfile(VARS, VM / "OVMF_VARS.fd")


def run(args: argparse.Namespace) -> None:
    if status():
        raise SystemExit("VM is already running. Use ./dev restart to rebuild and relaunch.")
    check(graphical=not args.headless)
    if not args.no_build:
        build()
    stage(args.ui.resolve())
    QMP.unlink(missing_ok=True)
    command = [
        "qemu-system-x86_64", "-name", "Limine-Eucleia | GUI development",
        "-machine", "q35", "-accel", accelerator(),
        "-cpu", "host" if accelerator() == "kvm" else "max",
        "-m", "512M", "-smp", "2", "-nic", "none",
        "-device", "virtio-tablet-pci,id=pointer",
        "-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={CODE}",
        "-drive", f"if=pflash,format=raw,unit=1,file={VM / 'OVMF_VARS.fd'}",
        "-drive", f"format=raw,if=virtio,readonly=on,file={VM / 'esp.img'}",
        "-vga", "std", "-display", "none" if args.headless else "gtk,gl=off",
        "-qmp", f"unix:{QMP},server=on,wait=off", "-monitor", "none",
        "-serial", f"file:{LOGS / 'serial.log'}",
        "-debugcon", f"file:{LOGS / 'debugcon.log'}",
        "-global", "isa-debugcon.iobase=0xe9", "-boot", "menu=off",
    ]
    if args.debug:
        command.extend(["-S", "-gdb", f"unix:{VM / 'gdb.sock'},server=on,wait=off"])
    (VM / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (LOGS / "qemu.log").open("w") as log:
        process = subprocess.Popen(command, env=environment(), stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=log, start_new_session=True)
    (VM / "qemu.pid").write_text(str(process.pid) + "\n")
    for _ in range(100):
        if process.poll() is not None:
            raise SystemExit((LOGS / "qemu.log").read_text())
        if status():
            print(f"VM started (PID {process.pid}). ./dev screenshot captures its framebuffer.")
            return
        time.sleep(0.1)
    raise SystemExit(f"QMP did not become ready; inspect {LOGS / 'qemu.log'}")


def stop() -> None:
    if status():
        qmp("quit")
        for _ in range(50):
            if status() is None:
                break
            time.sleep(0.1)
        else:
            raise SystemExit("VM has not stopped yet; retry shortly.")
    print("VM stopped.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "build", "run", "restart", "stop",
                                            "status", "screenshot", "key"))
    parser.add_argument("value", nargs="?", help="Screenshot path or QEMU key name")
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--debug", action="store_true", help="Pause at reset with a GDB socket")
    parser.add_argument("--ui", type=Path, default=ROOT / "themes/classical/eucleia.conf",
                        help="UI configuration to stage with its referenced assets")
    parser.add_argument("--no-build", action="store_true", help="Reuse binaries and theme pack for UI edits")
    args = parser.parse_args()
    for directory in (WORK, BUILD, VM, LOGS):
        directory.mkdir(parents=True, exist_ok=True)
    with (WORK / "dev.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if args.command == "check":
            check()
        elif args.command == "build":
            build()
        elif args.command in ("run", "restart"):
            if args.command == "restart":
                stop()
            run(args)
        elif args.command == "stop":
            stop()
        elif args.command == "status":
            print(json.dumps(status() or {"status": "stopped"}, indent=2))
        elif args.command == "screenshot":
            path = Path(args.value).resolve() if args.value else VM / "screenshot.png"
            path.parent.mkdir(parents=True, exist_ok=True)
            qmp("screendump", {"filename": str(path), "format": "png"})
            print(path)
        elif args.command == "key":
            if not args.value:
                parser.error("key requires a QEMU key name, such as down, up, ret or esc")
            qmp("send-key", {"keys": [{"type": "qcode", "data": args.value}]})


if __name__ == "__main__":
    try:
        main()
    except (OSError, EOFError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
