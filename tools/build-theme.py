#!/usr/bin/env python3
"""Pack artwork, TrueType font files and layout tokens for the boot GUI."""

from __future__ import annotations

import argparse
import io
import json
import struct
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / "themes/classical"

def font_asset(path: Path, size: int, weight: float) -> tuple[int, int, int, bytes]:
    font = TTFont(path, recalcTimestamp=False)
    if "glyf" not in font:
        raise ValueError("Use a TrueType font with glyf outlines")
    if "fvar" in font:
        axes = {axis.axisTag: axis.defaultValue for axis in font["fvar"].axes}
        for axis in font["fvar"].axes:
            if axis.axisTag == "wght":
                if not axis.minValue <= weight <= axis.maxValue:
                    raise ValueError("Weight is outside the font's variable axis")
                axes["wght"] = weight
        font = instantiateVariableFont(font, axes)
    output = io.BytesIO()
    font.save(output)
    font.close()
    data = output.getvalue()
    if not 4 <= size <= 128 or not 12 <= len(data) <= 2 * 1024 * 1024:
        raise ValueError("Font size or file exceeds the runtime limit")
    if data[:4] not in (b"\x00\x01\x00\x00", b"true"):
        raise ValueError("Use a TrueType font with glyf outlines")
    return 3, size, 0, data


def image_asset(path: Path) -> tuple[int, int, int, bytes]:
    with Image.open(path) as source, source.convert("RGBA") as im:
        return 1, im.width, im.height, im.tobytes("raw", "BGRA")


def build(kit: Path, tokens: Path) -> bytearray:
    t = json.loads(tokens.read_text())
    layout_tokens, fonts, colours = t["layout"], t["typography"], t["colours"]
    assets = [image_asset(kit / t["background"])]
    for role, path in [("heading", "cinzel/Cinzel[wght].ttf"),
                       ("label", "ebgaramond/EBGaramond[wght].ttf"),
                       ("footer", "ebgaramond/EBGaramond[wght].ttf")]:
        assets.append(font_asset(kit / "fonts" / fonts[role].get("file", path),
                                 fonts[role]["size"], fonts[role].get("weight", 400)))
    for path in ["components/ornamental-divider.png", "icons/boot-enter.png",
                 "icons/snapshots.png", "icons/chevron-right.png"]:
        assets.append(image_asset(kit / path))
    layout = [t["canvas"]["width"], t["canvas"]["height"],
              layout_tokens["content_x"], layout_tokens["content_width"],
              layout_tokens["wordmark_y"] + fonts["heading"]["baseline"],
              layout_tokens["menu_y"], layout_tokens["row_height"], layout_tokens["row_gap"],
              layout_tokens["icon_x"], layout_tokens["icon_size"],
              layout_tokens["label_x"], layout_tokens["label_baseline"],
              layout_tokens["chevron_x"], layout_tokens["chevron_size"],
              layout_tokens["divider_x"], layout_tokens["divider_y"],
              layout_tokens["divider_width"], layout_tokens["divider_height"],
              layout_tokens["footer_y"], layout_tokens["label_max_width"]]
    layout += [int(colours[key].lstrip("#"), 16) for key in
               ["ivory", "ivory_selected", "bronze", "muted", "ink"]]
    layout += [round(t["states"]["selected"]["fill_opacity"] * 255)]
    layout += [round(fonts[role]["tracking"] * 64) for role in ["heading", "label", "footer"]]
    runtime = t.get("runtime", {})
    layout += [runtime.get("max_rows", 5), runtime.get("footer_baseline", 26),
               runtime.get("status_y", 880)]
    assert len(layout) == 32
    layout_offset = 32 + len(assets) * 32
    data = bytearray(layout_offset) + struct.pack("<32I", *layout)
    for index, (kind, width, height, pixels) in enumerate(assets):
        data += bytes((-len(data)) % 4)
        offset = len(data)
        data += pixels
        struct.pack_into("<8I", data, 32 + index * 32, kind, index, width, height,
                         offset, len(pixels), 0, 0)
    struct.pack_into("<8s6I", data, 0, b"EUCTHM02", 2, len(data), len(assets), layout_offset, 0, 0)
    if len(data) > 16 * 1024 * 1024:
        raise ValueError("Theme exceeds the 16 MiB runtime limit")
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kit", type=Path, default=KIT)
    parser.add_argument("--tokens", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "work/theme/eucleia.eui")
    args = parser.parse_args()
    result = build(args.kit, args.tokens or args.kit / "tokens.json")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"Built {args.output} ({len(result):,} bytes)")


if __name__ == "__main__":
    main()
