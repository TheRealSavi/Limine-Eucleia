#!/usr/bin/env python3
"""Check eucleia.conf discovery, appearance, assets and preserved boot behaviour."""

from __future__ import annotations

import json
import time
from collections.abc import Generator, Sequence
from contextlib import contextmanager
from pathlib import Path

from menu_vm_test import ENTRIES, HEADER, ROOT, VM
from mouse_vm_test import click, move
from PIL import Image, ImageChops

WORK = ROOT / "work/authoring/vm"
BINARY = ROOT / "work/build/bin/BOOTX64.EFI"
BIOS = ROOT / "work/menu-refactor/build-bios/bin"
MINIMAL = (ROOT / "examples/minimal.conf").read_text()
DEFAULT = "version: 1\ntheme: /boot/eucleia.eui\n"
passed: list[str] = []


@contextmanager
def guest(name: str, ui: str | None, config: str = HEADER + ENTRIES,
          files: Sequence[tuple[Path, str]] = (), ready: bool = True, *,
          bios: Path | None = None, memory: int = 512,
          config_directory: str = "/") -> Generator[VM, None, None]:
    vm = VM(WORK / name, BINARY, config, ui_config=ui,
            extra_files=[(ROOT / "work/theme/eucleia.eui", "::/boot/eucleia.eui"), *files],
            extra_qemu=[] if bios else ["-device", "virtio-tablet-pci,id=pointer"],
            bios=bios, memory=memory, config_directory=config_directory)
    try:
        if ready:
            vm.menu_ready()
            time.sleep(0.8)
        yield vm
        passed.append(name)
        print("Passed:", name, flush=True)
    finally:
        vm.close()


def same(a: Path, b: Path, notice: bool = False) -> None:
    with Image.open(a).convert("RGB") as first, Image.open(b).convert("RGB") as second:
        diff = ImageChops.difference(first, second).getbbox()
        assert first.size == second.size and (diff is None or (notice and diff[1] >= 64 and diff[3] <= 80)), (a, b, diff)


def main() -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    with guest("absent", None) as vm:
        terminal = vm.capture("menu")
        vm.key("ret")
        vm.booted("fixture-a")
    with guest("disabled", "enabled: no\n") as vm:
        same(terminal, vm.capture("menu"))
        vm.key("ret")
        vm.booted("fixture-a")
    with guest("classical", DEFAULT) as vm:
        classical = vm.capture("menu")
        vm.key("ret")
        vm.booted("fixture-a")
    with guest("minimal", MINIMAL) as vm:
        first = vm.capture("menu")
        with Image.open(first).convert("RGB") as image:
            assert image.getpixel((20, 20)) == (16, 24, 32)
            assert image.getpixel((650, 215)) == (40, 75, 94)
        with Image.open(classical).convert("RGB") as a, Image.open(first).convert("RGB") as b:
            assert ImageChops.difference(a, b).getbbox() is not None
        vm.key("e")
        vm.key("esc")
        same(first, vm.capture("after-editor"))
        move(vm, 750, 311)
        click(vm)
        vm.capture("submenu")
        move(vm, 750, 387)
        click(vm)
        vm.booted("fixture-child")
    marker = WORK / "icon.png"
    Image.new("RGBA", (4, 4), (250, 20, 30, 255)).save(marker)
    decoy = WORK / "decoy.conf"
    decoy.write_text("enabled: no\n")
    mapped = MINIMAL.replace("[menu.item.label]\n", "[menu.item.label]\nfont: label.ttf\n")
    mapped += '\n[entry "Boot A"]\nicon: icon.png\n[entry "Group/Child"]\nicon: icon.png\n'
    with guest("relative-assets", mapped, config_directory="/boot/limine/", files=[
        (marker, "::/boot/limine/icon.png"), (ROOT / "work/fonts/heading.ttf", "::/boot/limine/label.ttf"),
        (decoy, "::/eucleia.conf")]) as vm:
        with Image.open(vm.capture("menu")).convert("RGB") as image:
            assert image.getpixel((612, 235)) == (250, 20, 30)
        vm.key("down")
        vm.key("ret")
        vm.key("down")
        with Image.open(vm.capture("child-icon")).convert("RGB") as image:
            assert image.getpixel((612, 387)) == (250, 20, 30)
        vm.key("ret")
        vm.booted("fixture-child")
    for name, config in [("unknown-property", DEFAULT + "timeout: 0\n"),
                         ("invalid-geometry", DEFAULT + "[menu]\nwidth: 10px\n"),
                         ("invalid-font", DEFAULT + "[heading]\nfont: missing.ttf\n"),
                         ("missing-header", DEFAULT + "[heading]\nmode: image\nfile: missing.png\nheight: 100px\n"),
                         ("invalid-header", DEFAULT + "[heading]\nmode: image\nfile: /boot/eucleia.eui\nheight: 100px\n")]:
        with guest(name, config) as vm:
            same(terminal, vm.capture("fallback"), notice=True)
            vm.key("ret")
            vm.booted("fixture-a")
    with guest("missing-icon", MINIMAL + '\n[entry "Boot A"]\nicon: missing.png\n') as vm:
        with Image.open(vm.capture("menu")).convert("RGB") as image:
            assert image.getpixel((20, 20)) == (16, 24, 32)
        vm.key("ret")
        vm.booted("fixture-a")
    wallpaper = WORK / "wallpaper.png"
    Image.new("RGBA", (2, 4), (5, 20, 60, 128)).save(wallpaper)
    config = MINIMAL.replace("visible: no\ncolour:", "visible: yes\nfile: wallpaper.png\nfit: contain\ncolour:")
    with guest("background-png", config, files=[(wallpaper, "::/wallpaper.png")]) as vm:
        with Image.open(vm.capture("menu")).convert("RGB") as image:
            assert image.getpixel((20, 20)) == (16, 24, 32)
            assert image.getpixel((500, 20)) == (10, 22, 46)
        vm.key("ret")
        vm.booted("fixture-a")
    with guest("positioned-countdown", MINIMAL,
               config=HEADER.replace("timeout: no", "timeout: 30") + ENTRIES) as vm:
        with Image.open(vm.capture("countdown")).convert("RGB") as a, Image.open(first).convert("RGB") as b:
            diff = ImageChops.difference(a, b).getbbox()
            assert diff and diff[0] >= 560 and diff[1] >= 600 and diff[3] < 640, diff
        vm.key("home")
        same(first, vm.capture("cancelled"))
        vm.key("ret")
        vm.booted("fixture-a")
    for bios in [False, True]:
        with guest("minimal-256-" + ("bios" if bios else "uefi"), MINIMAL, memory=256,
                   bios=BIOS if bios else None) as vm:
            vm.capture("menu")
            vm.key("ret")
            vm.booted("fixture-a")
    boot = HEADER.replace("timeout: no", "timeout: 0.5") + ENTRIES
    with guest("limine-timeout", MINIMAL, config=boot, ready=False) as vm:
        vm.booted("fixture-a")
    (WORK / "results.json").write_text(json.dumps({"passed": passed, "count": len(passed)}, indent=2) + "\n")
    print(f"All {len(passed)} authoring fixtures passed.", flush=True)


if __name__ == "__main__":
    main()
