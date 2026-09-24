#!/usr/bin/env python3
"""Exercise themed key help and selected-entry descriptions in real firmware."""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import cast

from menu_vm_test import ENTRIES, HEADER, ROOT, VM
from PIL import Image, ImageChops

WORK = ROOT / "work/hints/vm"
BINARY = ROOT / "work/build/bin/BOOTX64.EFI"
BIOS = ROOT / "work/menu-refactor/build-bios/bin"
KIT = ROOT / "themes/classical"
COMMENT = "Start the primary system with your saved boot options. Edit the entry to change kernel arguments for this boot only."


def same(a: Path, b: Path) -> None:
    with Image.open(a).convert("RGB") as first, Image.open(b).convert("RGB") as second:
        assert ImageChops.difference(first, second).getbbox() is None, (a, b)


def main() -> None:
    files = [(ROOT / "work/theme/eucleia.eui", "::/boot/eucleia.eui")]
    files += [(path, "::/components/" + path.name) for path in (KIT / "components").glob("*.png")]
    files += [(path, "::/icons/" + path.name) for path in (KIT / "icons").glob("*.png")]
    examples = [ROOT / "examples" / f"hints-{name}.conf" for name in ["grid", "custom", "vertical"]]
    examples += [KIT / "eucleia.conf"]
    cases = [(p, 1280, 720, False) for p in examples]
    cases += [(examples[0], 1920, 1080, False), (examples[0], 800, 600, False),
              (examples[0], 1280, 720, True)]
    passed: list[str] = []
    for path, width, height, bios in cases:
        name = f"{path.stem}-{'bios' if bios else 'uefi'}-{width}x{height}"
        config = HEADER.replace("1280x720", f"{width}x{height}") + ENTRIES.replace("First payload", COMMENT)
        vm = VM(WORK / name, BINARY, config, extra_files=files, ui_config=path.read_text(),
                bios=BIOS if bios else None, memory=256,
                video=["-vga", "none", "-device", f"VGA,xres={width},yres={height}"])
        try:
            vm.menu_ready(resolution=(width, height))
            deadline = time.monotonic() + 15
            while True:
                initial = vm.capture("menu")
                with Image.open(initial).convert("RGB") as image:
                    if path == examples[-1]:
                        scale = min(width / 1920, height / 1080)
                        red, green, blue = cast(tuple[int, int, int], image.getpixel((round(218 * scale), round(474 * scale))))
                        ready = red > 200 and red > green > blue
                    else:
                        ready = image.getpixel((10, 10)) == (16, 24, 32)
                if ready:
                    break
                assert time.monotonic() < deadline, name
                time.sleep(0.2)
            vm.key("down")
            group = vm.capture("directory")
            with Image.open(initial).convert("RGB") as a, Image.open(group).convert("RGB") as b:
                # Enter changes to Expand and the unavailable Edit hint disappears.
                box = (0, round(height * .8), width, height) if path != examples[2] else (
                    round(width * .62), round(height * .28), round(width * .94), round(height * .74))
                assert ImageChops.difference(a.crop(box), b.crop(box)).getbbox() is not None
            vm.key("end")
            vm.capture("last-entry-description")
            vm.key("home")
            same(initial, vm.capture("restored"))
            vm.key("e")
            vm.key("esc")
            same(initial, vm.capture("after-editor"))
            vm.key("ret")
            vm.booted("fixture-a")
            passed.append(name)
            print("Passed:", name, flush=True)
        finally:
            vm.close()
    (WORK / "results.json").write_text(json.dumps({"count": len(passed), "passed": passed}, indent=2) + "\n")


if __name__ == "__main__":
    main()
