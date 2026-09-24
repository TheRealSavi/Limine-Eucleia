#!/usr/bin/env python3
"""Verify real OVMF pointer delivery through QEMU VirtIO input devices."""

from __future__ import annotations

import argparse
import json
import time
from collections.abc import Generator, Sequence
from contextlib import contextmanager
from pathlib import Path

from menu_vm_test import ENTRIES, HEADER, ROOT, VM
from PIL import Image, ImageChops

WORK = ROOT / "work/mouse-research/integration"
BINARY = ROOT / "work/build/bin/BOOTX64.EFI"
GUI = "# Eucleia companion configuration\n"
TABLET = ["-device", "virtio-tablet-pci,id=pointer"]
passed: list[str] = []


@contextmanager
def guest(name: str, config: str, binary: Path = BINARY,
          resolution: tuple[int, int] = (1280, 720),
          device: Sequence[str] = TABLET) -> Generator[VM, None, None]:
    ui_config = "version: 1\ntheme: /boot/eucleia.eui\n" if GUI in config else None
    config = config.replace(GUI, "")
    vm = VM(WORK / name, binary, config, ui_config=ui_config,
            extra_files=[(ROOT / "work/theme/eucleia.eui", "::/boot/eucleia.eui")], extra_qemu=device)
    try:
        vm.menu_ready(resolution=resolution)
        time.sleep(0.5)
        yield vm
        passed.append(name)
        print(f"Passed: {name}", flush=True)
    finally:
        vm.close()


def move(vm: VM, x: float, y: float, width: int = 1280, height: int = 720) -> None:
    vm.qmp("input-send-event", {"events": [
        {"type": "abs", "data": {"axis": "x", "value": round(x * 32767 / width)}},
        {"type": "abs", "data": {"axis": "y", "value": round(y * 32767 / height)}}]})
    time.sleep(0.15)


def button(vm: VM, down: bool) -> None:
    vm.qmp("input-send-event", {"events": [
        {"type": "btn", "data": {"button": "left", "down": down}}]})
    time.sleep(0.15)


def click(vm: VM) -> None:
    button(vm, True)
    button(vm, False)


def row(vm: VM, index: int, width: int = 1280, height: int = 720) -> None:
    scale = min(width / 1920, height / 1080)
    move(vm, (width - 1920 * scale) / 2 + 600 * scale,
         (height - 1080 * scale) / 2 + (392 + index * 96) * scale, width, height)


def changed(before: Path, after: Path) -> None:
    with Image.open(before).convert("RGB") as a, Image.open(after).convert("RGB") as b:
        assert a.size == b.size and ImageChops.difference(a, b).getbbox() is not None


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path,
                        default=ROOT / "work/menu-refactor/baseline.BOOTX64.EFI")
    args = parser.parse_args()
    for width, height in [(1280, 720), (1920, 1080), (1024, 768)]:
        config = HEADER.replace("1280x720", f"{width}x{height}") + GUI + ENTRIES
        with guest(f"gui-submenu-{width}x{height}", config, resolution=(width, height)) as vm:
            before = vm.capture("initial")
            row(vm, 1, width, height)
            changed(before, vm.capture("hover"))
            click(vm)
            vm.capture("expanded")
            row(vm, 2, width, height)
            click(vm)
            vm.booted("fixture-child")
    with guest("editor-resume", HEADER + GUI + ENTRIES) as vm:
        row(vm, 0)
        vm.key("e")
        move(vm, 800, 500)
        vm.key("esc")
        row(vm, 2)
        click(vm)
        vm.booted("fixture-z")
    with guest("drag-across-rows", HEADER + GUI + ENTRIES) as vm:
        row(vm, 0)
        button(vm, True)
        row(vm, 2)
        button(vm, False)
        time.sleep(0.3)
        assert "We're alive" not in (vm.directory / "debugcon.log").read_text(errors="replace")
        click(vm)
        vm.booted("fixture-z")
    with guest("timeout-cancel", HEADER.replace("timeout: no", "timeout: 20") + GUI + ENTRIES) as vm:
        row(vm, 2)
        vm.capture("cancelled")
        time.sleep(21)
        assert "We're alive" not in (vm.directory / "debugcon.log").read_text(errors="replace")
        vm.capture("past-deadline")
        click(vm)
        vm.booted("fixture-z")
    broken = ENTRIES.replace("boot():/boot/test.elf", "boot():/missing.elf", 1)
    with guest("failed-boot-recovery", HEADER + GUI + broken) as vm:
        row(vm, 0)
        click(vm)
        time.sleep(0.5)
        vm.key("spc")
        vm.menu_ready()
        row(vm, 2)
        click(vm)
        vm.booted("fixture-z")
    one_entry = HEADER + "\n/Pointer boot\n protocol: limine\n path: boot():/boot/test.elf\n cmdline: pointer-boot\n"
    for name, binary in [("terminal-fallback", BINARY), ("unmodified-upstream", args.upstream.resolve())]:
        with guest(name, one_entry, binary=binary) as vm:
            before = vm.capture("initial")
            move(vm, 650, 360)
            changed(before, vm.capture("pointer"))
            click(vm)
            vm.booted("pointer-boot")
    with guest("relative-pointer", HEADER + GUI + ENTRIES,
               device=["-device", "virtio-mouse-pci,id=pointer"]) as vm:
        before = vm.capture("initial")
        for _ in range(6):
            vm.qmp("input-send-event", {"events": [
                {"type": "rel", "data": {"axis": "x", "value": 30}},
                {"type": "rel", "data": {"axis": "y", "value": 12}}]})
            time.sleep(0.1)
        changed(before, vm.capture("moved"))
        vm.key("home")
        vm.key("ret")
        vm.booted("fixture-a")
    (WORK / "results.json").write_text(json.dumps({"passed": passed, "count": len(passed)}, indent=2) + "\n")
    print(f"All {len(passed)} real-pointer fixtures passed.", flush=True)


if __name__ == "__main__":
    main()
