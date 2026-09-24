#!/usr/bin/env python3
"""Exercise the shipped image header and decorated rows in real firmware."""

from __future__ import annotations

import json
import shutil
import time
from pathlib import Path
from typing import cast

from menu_vm_test import ENTRIES, HEADER, ROOT, VM
from mouse_vm_test import click, move
from PIL import Image, ImageChops

KIT = ROOT / "themes/classical"
WORK = ROOT / "work/theme/vm"
UI = (KIT / "eucleia.conf").read_text()
BINARY = ROOT / "work/build/bin/BOOTX64.EFI"
BIOS = ROOT / "work/menu-refactor/build-bios/bin"


def same(a: Path, b: Path) -> None:
    with Image.open(a).convert("RGB") as first, Image.open(b).convert("RGB") as second:
        assert first.size == second.size and ImageChops.difference(first, second).getbbox() is None


def main() -> None:
    files = [(ROOT / "work/theme/eucleia.eui", "::/boot/eucleia.eui")]
    files += [(path, "::/" + path.relative_to(KIT).as_posix())
    for folder in ["icons", "components"] for path in (KIT / folder).glob("*.png")]
    passed: list[str] = []
    demo = (ROOT / "examples/limine.conf").read_text()
    for entry, keys in [("linux", []), ("windows", ["down"]),
                        ("recovery", ["down", "down"]),
                        ("shell", ["end", "ret", "down"])]:
        name = "demo-" + entry
        vm = VM(WORK / name, BINARY, demo, extra_files=files, ui_config=UI)
        try:
            vm.menu_ready()
            time.sleep(1)
            if entry == "linux":
                shutil.copyfile(vm.capture("menu"), WORK / "classical.png")
            for key in keys:
                vm.key(key)
            vm.capture("selected")
            vm.key("ret")
            vm.booted("eucleia-demo-" + entry)
            passed.append(name)
            print("Passed:", name, flush=True)
        finally:
            vm.close()
    cases = [(800, 600, False), (1280, 720, False), (1920, 1080, False),
             (2560, 1440, False), (720, 1280, False), (1280, 720, True)]
    for width, height, bios in cases:
        name = f"{'bios' if bios else 'uefi'}-{width}x{height}"
        config = HEADER.replace("1280x720", f"{width}x{height}") + ENTRIES
        vm = VM(WORK / name, BINARY, config, extra_files=files, ui_config=UI,
                bios=BIOS if bios else None, memory=256,
                video=["-vga", "none", "-device", f"VGA,xres={width},yres={height}"],
                extra_qemu=[] if bios else ["-device", "virtio-tablet-pci,id=pointer"])
        try:
            vm.menu_ready(resolution=(width, height))
            scale = min(width / 1920, height / 1080)
            mx, my = round(218 * scale), round(474 * scale)
            deadline = time.monotonic() + 15
            while True:
                initial = vm.capture("menu")
                with Image.open(initial).convert("RGB") as image:
                    red, green, blue = cast(tuple[int, int, int], image.getpixel((mx, my)))
                if red > 200 and red > green > blue:
                    break
                assert time.monotonic() < deadline, (name, (red, green, blue))
                time.sleep(0.2)
            vm.key("down")
            moved = vm.capture("selection-moved")
            with Image.open(initial).convert("RGB") as a, Image.open(moved).convert("RGB") as b:
                assert a.getpixel((mx, my)) != b.getpixel((mx, my))
                # Navigation must leave the cached header untouched.
                assert ImageChops.difference(a.crop((0, 0, width, round(400 * scale))),
                                            b.crop((0, 0, width, round(400 * scale)))).getbbox() is None
            vm.key("home")
            same(initial, vm.capture("selection-restored"))
            vm.key("e")
            vm.key("esc")
            same(initial, vm.capture("after-editor"))
            if not bios and width == 1280:
                move(vm, 800, 250)
                cursor = vm.capture("themed-cursor")
                with Image.open(initial).convert("RGB") as a, Image.open(cursor).convert("RGB") as b:
                    box = ImageChops.difference(a, b).getbbox()
                    assert box and box[0] >= 798 and box[1] >= 249 and box[2] <= 818 and box[3] <= 277, box
                move(vm, 900, 400)
                with Image.open(initial).convert("RGB") as a, Image.open(vm.capture("cursor-moved")).convert("RGB") as b:
                    assert ImageChops.difference(a.crop((798, 249, 818, 277)),
                                                b.crop((798, 249, 818, 277))).getbbox() is None
                move(vm, 400 * scale, 554 * scale, width, height)
                click(vm)
                vm.capture("expanded")
                move(vm, 400 * scale, 634 * scale, width, height)
                click(vm)
                vm.booted("fixture-child")
            else:
                vm.key("ret")
                vm.booted("fixture-a")
            passed.append(name)
            print("Passed:", name, flush=True)
        finally:
            vm.close()
    right = UI.replace("marker: diamond-left", "marker: diamond-right")
    vm = VM(WORK / "right-marker", BINARY, HEADER + ENTRIES, extra_files=files, ui_config=right)
    try:
        vm.menu_ready()
        time.sleep(1)
        with Image.open(vm.capture("menu")).convert("RGB") as image:
            red, green, blue = cast(tuple[int, int, int], image.getpixel((582, 316)))
            assert red > 200 and red > green > blue
        vm.key("ret")
        vm.booted("fixture-a")
        passed.append("right-marker")
    finally:
        vm.close()
    (WORK / "results.json").write_text(json.dumps({"passed": passed, "count": len(passed)}, indent=2) + "\n")
    print(f"All {len(passed)} theme fixtures passed.", flush=True)


if __name__ == "__main__":
    main()
