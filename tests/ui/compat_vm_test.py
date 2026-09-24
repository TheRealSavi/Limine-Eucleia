#!/usr/bin/env python3
"""Check existing Limine boot paths and display compatibility in private guests."""

from __future__ import annotations

import argparse
import json
import time
from collections.abc import Generator, Sequence
from contextlib import contextmanager
from pathlib import Path

from menu_vm_test import ENTRIES, HEADER, ROOT, VM
from mouse_vm_test import click, move
from PIL import Image, ImageChops

WORK = ROOT / "work/compatibility/vm"
BINARY = ROOT / "work/build/bin/BOOTX64.EFI"
BIOS = ROOT / "work/menu-refactor/build-bios/bin"
FIXTURES = ROOT / "work/compatibility/fixtures"
GUI = "# Eucleia companion configuration\n"
MENU = ENTRIES.replace("/Hidden BIOS entry\n    protocol: bios\n", "")
passed: list[str] = []


@contextmanager
def guest(name: str, config: str, resolution: tuple[int, int] = (1280, 720),
          bios: bool = False, files: Sequence[tuple[Path, str]] = (), *,
          extra_qemu: Sequence[str] = (), video: Sequence[str] = ("-vga", "std"),
          memory: int = 512, machine: str = "q35") -> Generator[VM, None, None]:
    extra = list(extra_qemu)
    if not bios:
        extra += ["-device", "virtio-tablet-pci,id=pointer"]
    ui_config = "version: 1\ntheme: /boot/eucleia.eui\n" if GUI in config else None
    config = config.replace(GUI, "")
    vm = VM(WORK / name, BINARY, config, ui_config=ui_config,
            extra_files=[(ROOT / "work/theme/eucleia.eui", "::/boot/eucleia.eui"), *files],
            bios=BIOS if bios else None, extra_qemu=extra, video=video, memory=memory, machine=machine)
    try:
        vm.menu_ready(resolution=resolution)
        deadline = time.monotonic() + 20
        while True:
            with Image.open(vm.capture("waiting")).convert("RGB") as screen:
                samples = {screen.getpixel((x * screen.width // 16, y * screen.height // 16))
                           for x in range(16) for y in range(16)}
            if len(samples) > 64:
                break
            assert time.monotonic() < deadline, f"Menu artwork did not appear: {vm.directory}"
            time.sleep(0.1)
        time.sleep(0.4)
        yield vm
        passed.append(name)
        print(f"Passed: {name}", flush=True)
    finally:
        vm.close()


def same(a: Path, b: Path, transpose: Image.Transpose | None = None) -> None:
    with Image.open(a).convert("RGB") as first, Image.open(b).convert("RGB") as second:
        if transpose is not None:
            first = first.transpose(transpose)
        assert first.size == second.size and ImageChops.difference(first, second).getbbox() is None, (a, b)


def wait_log(vm: VM, filename: str, marker: str, timeout: float = 45) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data = (vm.directory / filename).read_text(errors="replace")
        if marker in data:
            return data
        time.sleep(0.1)
    vm.capture("failure")
    raise AssertionError(f"Missing {marker}: {vm.directory / filename}")


def row(vm: VM, index: int, rotation: int = 0, width: int = 1280, height: int = 720) -> None:
    w, h = (height, width) if rotation in (90, 270) else (width, height)
    scale = min(w / 1920, h / 1080)
    x = round((w - 1920 * scale) / 2 + 600 * scale)
    y = round((h - 1080 * scale) / 2 + (392 + index * 96) * scale)
    if rotation == 90:
        x, y = width - 1 - y, x
    elif rotation == 180:
        x, y = width - 1 - x, height - 1 - y
    elif rotation == 270:
        x, y = y, height - 1 - x
    move(vm, x, y, width, height)


def displays() -> None:
    with guest("reference", HEADER + GUI + MENU) as vm:
        reference = vm.capture("menu")
    portrait_config = HEADER.replace("1280x720", "720x1280") + GUI + MENU
    with guest("portrait-reference", portrait_config, resolution=(720, 1280),
               video=["-vga", "none", "-device", "VGA,xres=720,yres=1280"]) as vm:
        portrait = vm.capture("menu")
        vm.key("ret")
        vm.booted("fixture-a")
    for rotation, transpose in [(90, Image.Transpose.ROTATE_270),
                                (180, Image.Transpose.ROTATE_180),
                                (270, Image.Transpose.ROTATE_90)]:
        config = f"interface_rotation: {rotation}\n" + HEADER + GUI + MENU
        with guest(f"rotation-{rotation}", config) as vm:
            initial = vm.capture("menu")
            same(portrait if rotation in (90, 270) else reference, initial, transpose)
            vm.key("e")
            vm.key("esc")
            same(initial, vm.capture("after-editor"))
            row(vm, 1, rotation)
            click(vm)
            vm.capture("submenu")
            row(vm, 2, rotation)
            click(vm)
            vm.booted("fixture-child")
    with guest("multiple-displays", HEADER + GUI + MENU,
               video=["-vga", "none", "-device", "VGA,id=primary",
                      "-device", "secondary-vga,id=secondary"]) as vm:
        same(reference, vm.capture("primary"))
        secondary = vm.directory / "secondary.png"
        vm.qmp("screendump", {"filename": str(secondary), "format": "png", "device": "secondary"})
        with Image.open(secondary).convert("RGB") as screen:
            assert screen.getbbox() is None
        vm.key("e")
        vm.qmp("screendump", {"filename": str(vm.directory / "secondary-editor.png"),
                              "format": "png", "device": "secondary"})
        vm.key("esc")
        same(reference, vm.capture("after-editor"))
        vm.qmp("screendump", {"filename": str(secondary), "format": "png", "device": "secondary"})
        with Image.open(secondary).convert("RGB") as screen:
            assert screen.getbbox() is None
        row(vm, 0)
        click(vm)
        vm.booted("fixture-a")
        wait_log(vm, "debugcon.log", "2 framebuffer(s)")
    for adapter in ["bochs-display"]:
        with guest(f"adapter-{adapter}", HEADER + GUI + MENU,
                   video=["-vga", "none", "-device", adapter]) as vm:
            same(reference, vm.capture("menu"))
            row(vm, 0)
            click(vm)
            vm.booted("fixture-a")


def relative(vm: VM, x: int, y: int) -> None:
    while x != 0 or y != 0:
        dx, dy = max(-100, min(100, x)), max(-100, min(100, y))
        vm.qmp("input-send-event", {"events": [
            {"type": "rel", "data": {"axis": "x", "value": dx}},
            {"type": "rel", "data": {"axis": "y", "value": dy}}]})
        x -= dx
        y -= dy
        time.sleep(0.03)


def bios_tests() -> None:
    for machine in ["q35", "pc"]:
        with guest(f"bios-{machine}", HEADER + GUI + MENU, bios=True, machine=machine) as vm:
            initial = vm.capture("menu")
            vm.key("e")
            vm.key("esc")
            same(initial, vm.capture("after-editor"))
            relative(vm, -2000, -2000)
            relative(vm, 400, 325)
            click(vm)
            vm.capture("submenu")
            relative(vm, 0, 64)
            click(vm)
            vm.booted("fixture-child")
    for name, option in [("terminal", ""), ("rotation", "interface_rotation: 90\n" + GUI)]:
        with guest(f"bios-{name}", HEADER + option + MENU, bios=True) as vm:
            vm.capture("menu")
            vm.key("ret")
            vm.booted("fixture-a")


def memory_tests() -> None:
    for bios in [False, True]:
        with guest(f"{'bios' if bios else 'uefi'}-256mb", HEADER + GUI + MENU,
                   bios=bios, memory=256) as vm:
            initial = vm.capture("menu")
            vm.key("e")
            vm.key("esc")
            same(initial, vm.capture("after-editor"))
            vm.key("down")
            vm.key("ret")
            vm.key("down")
            vm.key("ret")
            vm.booted("fixture-child")


def boots(kernels: Sequence[Path]) -> None:
    entry = "\n/EFI fixture\n protocol: efi\n path: boot():/boot/fixture.efi\n cmdline: eucleia-chainload\n"
    for renderer in ["gui", "terminal"]:
        with guest(f"efi-{renderer}", HEADER + (GUI if renderer == "gui" else "") + entry,
                   files=[(FIXTURES / "chainload.efi", "::/boot/fixture.efi")]) as vm:
            vm.key("ret")
            wait_log(vm, "debugcon.log", "EUCLEIA_EFI_OPTIONS_OK")
            vm.capture("payload")
    for index, kernel in enumerate(kernels):
        config = HEADER + GUI + "\n/Linux fixture\n protocol: linux\n path: boot():/boot/vmlinuz\n"
        config += " module_path: boot():/boot/initramfs.cpio\n cmdline: console=ttyS0,115200 rdinit=/init panic=-1\n"
        for bios in [False, True]:
            name = f"linux-{index}-{'bios' if bios else 'uefi'}"
            with guest(name, config, bios=bios, files=[(kernel, "::/boot/vmlinuz"),
                       (FIXTURES / "initramfs.cpio", "::/boot/initramfs.cpio")]) as vm:
                vm.key("ret")
                wait_log(vm, "serial.log", "EUCLEIA_LINUX_INIT_OK")
                (vm.directory / "kernel-source.txt").write_text(str(kernel) + "\n")
                vm.capture("payload")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=["all", "displays", "bios", "memory", "boot"], default="all")
    parser.add_argument("--kernel", type=Path, action="append", help="Read-only Linux kernel copy source; repeatable")
    args = parser.parse_args()
    kernels = args.kernel or sorted(Path("/usr/lib/modules").glob("*/vmlinuz"))
    if args.suite in ("all", "boot"):
        assert kernels, "Supply --kernel /path/to/vmlinuz"
    if args.suite in ("all", "displays"):
        displays()
    if args.suite in ("all", "bios"):
        bios_tests()
    if args.suite in ("all", "memory"):
        memory_tests()
    if args.suite in ("all", "boot"):
        boots(kernels)
    (WORK / f"results-{args.suite}.json").write_text(json.dumps({"passed": passed,
        "count": len(passed), "kernels": [str(k) for k in kernels]}, indent=2) + "\n")
    print(f"All {len(passed)} compatibility fixtures passed.", flush=True)


if __name__ == "__main__":
    main()
