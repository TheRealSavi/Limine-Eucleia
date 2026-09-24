#!/usr/bin/env python3
"""Exercise the framebuffer GUI and recovery paths in isolated QEMU guests."""

from __future__ import annotations

import io
import json
import struct
import time
from collections.abc import Generator
from contextlib import contextmanager
from pathlib import Path

from fontTools.ttLib import TTFont
from menu_vm_test import ENTRIES, HEADER, ROOT, VM
from PIL import Image, ImageChops

WORK = ROOT / "work/gui/vm"
BINARY = ROOT / "work/build/bin/BOOTX64.EFI"
THEME = ROOT / "work/theme/eucleia.eui"
GUI = "# Eucleia companion configuration\n"
passed: list[str] = []


@contextmanager
def guest(name: str, config: str, resolution: tuple[int, int] = (1280, 720),
          theme: Path | None = THEME, ready: bool = True) -> Generator[VM, None, None]:
    files = [] if theme is None else [(theme, "::/boot/eucleia.eui")]
    ui_config = "version: 1\ntheme: /boot/eucleia.eui\n" if GUI in config else None
    config = config.replace(GUI, "")
    vm = VM(WORK / name, BINARY, config, extra_files=files, ui_config=ui_config)
    try:
        if ready:
            vm.menu_ready(resolution=resolution)
            time.sleep(0.4)
        yield vm
        passed.append(name)
        print(f"Passed: {name}", flush=True)
    finally:
        vm.close()


def same(a: Path, b: Path, notice: bool = False) -> None:
    with Image.open(a).convert("RGB") as x, Image.open(b).convert("RGB") as y:
        difference = ImageChops.difference(x, y).getbbox()
        assert x.size == y.size and (difference is None or
            (notice and difference[1] >= 64 and difference[3] <= 80)), (a, b, difference)


def main() -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    with guest("navigation", HEADER + GUI + ENTRIES) as vm:
        vm.capture("initial")
        for key in ["up", "down", "end", "home", "down", "ret", "down", "down", "right"]:
            vm.key(key)
        vm.capture("nested")
        vm.key("home")
        before = vm.capture("before-editor")
        vm.key("e")
        vm.capture("editor")
        vm.key("esc")
        same(before, vm.capture("after-editor"))
        vm.key("b")
        vm.key("esc")
        same(before, vm.capture("after-blank-editor"))
        vm.key("e")
        vm.text("cmdline: gui-edited\n")
        vm.key("f10")
        vm.booted("gui-edited")
    for width, height in [(1920,1080), (2560,1440), (1024,768), (800,600)]:
        config = HEADER.replace("1280x720", f"{width}x{height}") + GUI + ENTRIES
        with guest(f"mode-{width}x{height}", config, (width,height)) as vm:
            vm.capture("menu")
            vm.key("ret")
            vm.booted("fixture-a")
    long_entries = "".join(f"\n/Entry {i:02d} {'long-name-' * 20 if i == 39 else ''}\n"
        f" protocol: limine\n path: boot():/boot/test.elf\n cmdline: gui-{i}\n" for i in range(40))
    with guest("scroll", HEADER + GUI + long_entries) as vm:
        vm.key("end")
        vm.capture("scrolled-long-label")
        vm.key("home")
        vm.key("3")
        vm.booted("gui-2")
    with guest("countdown", HEADER.replace("timeout: no", "timeout: 30") + GUI + ENTRIES) as vm:
        vm.capture("countdown")
        vm.key("down")
        vm.capture("cancelled")
        vm.key("home")
        vm.key("ret")
        vm.booted("fixture-a")
    for name, prefix, marker in [
        ("autoboot", "timeout: 0.25\n", "fixture-a"),
        ("immediate", "timeout: 0\n", "fixture-a"),
        ("quiet-boot", "quiet: yes\ntimeout: 0.25\n", "fixture-a"),
        ("nested-default", "default_entry: Group/Nested/Grandchild\ntimeout: 0\n", "fixture-grandchild")
    ]:
        with guest(name, prefix + HEADER.split("\n",1)[1] + GUI + ENTRIES, ready=False) as vm:
            vm.booted(marker)
    with guest("quiet-cancel", "quiet: yes\n" + HEADER.replace("timeout: no", "timeout: 30") + GUI + ENTRIES,
               ready=False) as vm:
        time.sleep(4)
        vm.key("x")
        vm.menu_ready()
        vm.capture("revealed")
        vm.key("ret")
        vm.booted("fixture-a")
    with guest("empty", HEADER + GUI) as vm:
        initial = vm.capture("empty")
        vm.key("b")
        vm.key("esc")
        same(initial, vm.capture("returned"))
    broken = ENTRIES.replace("boot():/boot/test.elf", "boot():/missing.elf", 1)
    with guest("recovery", HEADER + GUI + broken) as vm:
        for key in ["ret", "e"]:
            vm.key(key)
            if key == "e":
                vm.key("f10")
            time.sleep(1)
            vm.key("spc")
            vm.menu_ready()
            vm.capture("editor-return" if key == "e" else "menu-return")
        vm.key("esc")
        vm.key("end")
        vm.key("ret")
        vm.booted("fixture-z")
    with guest("terminal-reference", HEADER + ENTRIES, theme=None) as vm:
        terminal = vm.capture("menu")
    bad = WORK / "invalid.eui"
    bad.write_bytes(b"invalid theme pack")
    huge = WORK / "oversize.eui"
    with huge.open("wb") as f:
        f.truncate(16 * 1024 * 1024 + 1)
    broken_font = WORK / "broken-font.eui"
    font_data = bytearray(THEME.read_bytes())
    font_offset = struct.unpack_from("<I", font_data, 64 + 16)[0]
    font_data[font_offset:font_offset + 12] = bytes(12)
    broken_font.write_bytes(font_data)
    for name, theme in [("missing-theme",None), ("invalid-theme",bad), ("oversize-theme",huge),
                        ("broken-font",broken_font)]:
        with guest(name, HEADER + GUI + ENTRIES, theme=theme) as vm:
            same(terminal, vm.capture("fallback"), notice=True)
            vm.key("ret")
            vm.booted("fixture-a")
    late = bytearray(THEME.read_bytes())
    offset, length = struct.unpack_from("<2I", late, 32 + 2 * 32 + 16)
    font = TTFont(io.BytesIO(late[offset:offset + length]), recalcTimestamp=False)
    cmap = font.getBestCmap()
    assert cmap is not None, "Font fixture has no Unicode character map"
    glyph_name = cmap[0x3a9]
    font["hmtx"].metrics[glyph_name] = (65535, 0)
    stream = io.BytesIO()
    font.save(stream)
    font.close()
    late += bytes((-len(late)) % 4)
    struct.pack_into("<2I", late, 32 + 2 * 32 + 16, len(late), len(stream.getvalue()))
    late += stream.getvalue()
    struct.pack_into("<I", late, 12, len(late))
    late_pack = WORK / "bad-glyph.eui"
    late_pack.write_bytes(late)
    late_entries = ENTRIES.replace("/Boot A", "/Boot A \u03a9")
    with guest("late-font-reference", HEADER + late_entries, theme=None) as vm:
        late_reference = vm.capture("menu")
    with guest("late-font-failure", HEADER + GUI + late_entries, theme=late_pack) as vm:
        same(late_reference, vm.capture("fallback"), notice=True)
        for key in ["down", "ret", "home", "ret"]:
            vm.key(key)
        vm.booted("fixture-a")
    for name, option, resolution in [("small-screen", "interface_resolution: 640x480\n", (640,480))]:
        base = HEADER.replace("interface_resolution: 1280x720\n", "") if name == "small-screen" else HEADER
        with guest(name + "-reference", option + base + ENTRIES, resolution) as vm:
            reference = vm.capture("menu")
        with guest(name, option + base + GUI + ENTRIES, resolution) as vm:
            same(reference, vm.capture("fallback"))
    huge.unlink()
    (WORK / "results.json").write_text(json.dumps({"passed":passed,"count":len(passed)},indent=2)+"\n")
    print(f"All {len(passed)} GUI/compatibility fixtures passed.", flush=True)


if __name__ == "__main__":
    main()
