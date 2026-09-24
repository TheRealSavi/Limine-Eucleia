# Native font rendering

The graphical menu uses FreeType 2.14.3 to rasterise TrueType outlines at the
actual display pixel size. The original Cinzel and EB Garamond files remain in
`themes/classical/fonts`, together with their SIL Open Font Licences.

## Pipeline

1. `tools/build-theme.py` uses FontTools to select the configured
   weight from variable fonts and package static TrueType outlines. Other axes
   use their defaults. This also resolves variable GPOS adjustments that
   FreeType's basic kerning API does not evaluate. Source fonts are untouched.
2. `common/ui/font.c` opens a memory face at the final integer pixel size.
   FreeType's light auto-hinting aligns vertical features to the pixel grid;
   its grayscale rasteriser generates coverage masks. No LCD RGB assumptions
   are made, so the same masks work on RGB and BGR framebuffers.
3. A cache holds 128 glyphs per face. The compositor places these masks at one
   common baseline, without resampling. Measurement and drawing both use
   1/64-pixel advances, pair kerning and tracking, with a shared rounding rule.
4. Text is clipped to its label box. Long labels reserve space for an ellipsis.
   Unsupported codepoints and malformed UTF-8 use `?`.

Coverage comes from each font's Unicode character map, rather than a baked
Latin-1 list. This is still simple horizontal text: FreeType's `kern` and basic
GPOS pair kerning are enabled, but contextual shaping, combining-mark
positioning, bidirectional layout and fallback font families are not provided.
Those would require a text-shaping layer such as HarfBuzz. CFF/CFF2 OpenType,
colour emoji and bitmap-only fonts are not enabled in this build.

## Change a font

Use a token file with `typography.heading`, `typography.label` and
`typography.footer`. Each supports:

- `file`: a TrueType file path relative to the kit's `fonts` directory. The
  defaults select Cinzel for the heading and EB Garamond for labels and hints.
- `size`: 4 to 128 pixels on the reference canvas.
- `weight`: the variable font's weight coordinate; the supplied fonts use 400.
  For a static font, choose the file for the desired weight.
- `tracking`: additional spacing in reference pixels.

The renderer rounds the scaled font size to the nearest whole pixel. The
existing minimum text scale remains two-thirds of the reference, and the
maximum rasterisation size is 256 pixels. The development default produces
48px headings, 23px labels and 14px hints at 1280x720.

```sh
python3 tools/build-theme.py --tokens work/my-theme.json
./dev restart
```

The builder needs Pillow and FontTools, both available in the development
system. `./dev check` checks their imports. Rebuild old packs: `EUCTHM02` stores
font outlines; `EUCTHM01` packs select the terminal fallback. Retain the font
copyright and licence files when distributing a theme. Custom fonts' licences
must permit any generated static instances and their redistribution.

## Integration and bounds

FreeType is fetched unchanged by `bootstrap`, pinned to commit
`0a0221a1347e2f1e07c395263540026e9a0aa7c7`. `./dev build` fetches it if absent
from an existing development checkout. `common/ui/freetype` contains our
configuration and runtime adapters. The enabled modules are base, TrueType,
SFNT, grayscale rendering, auto-hinting and PostScript glyph names. No OS
filesystem, host allocator, compression, PNG, SVG or HarfBuzz dependency is
linked into the loader.

The configuration also omits the Adobe glyph-name-to-Unicode list, native
TrueType bytecode interpreter and runtime variable-font support. Theme fonts
have Unicode character maps, use forced auto-hinting, and are instanced by the
host builder. These unused features otherwise consume BIOS conventional
memory. The supplied theme remains pixel-identical after this reduction.
The BIOS linker reserves room for disk I/O below the conventional-memory limit;
see [COMPATIBILITY.md](COMPATIBILITY.md) for the measured image size and guard.

Each font owns a 4 MiB arena for FreeType and its cached masks. Three roles add
12 MiB plus small metadata. Details shares the hints face by default; an
independent file or pixel size adds a fourth 4 MiB arena. Font input is limited to 2 MiB per face; glyph
bitmaps to 256x256; total theme size remains 16 MiB. The arena packs and reuses
small allocations, then releases all font storage together. If its budget is
exhausted, the adapter aborts the face through its guarded entry point. It does
not ask FreeType's auto-hinter to continue with a failed internal allocation.
This also avoids relying on partially constructed FreeType objects for cleanup.
The bootloader's outer page allocator still has its existing global OOM policy.

The recovery boundary uses compiler context-save builtins on x86. AArch64,
RISC-V64 and LoongArch64 use the small adapter in `common/ui/freetype/jump.S`,
which saves the callee-preserved integer registers, stack and return address.
This adapter relies on the integer-only ABIs already selected for those Limine
targets. It is not a general-purpose floating-point context implementation.

ASCII UI glyphs and the ellipsis are checked during face initialisation. A bad
font makes the renderer fall back to the terminal. A later glyph/render failure
switches the menu to the terminal on drawing that view. Selection, timeout
policy, editing and boot execution stay in the existing controller.

## Licensing

Limine retains its BSD-2-Clause licence. FreeType is used under the FreeType
License (FTL), its permissive BSD-style option with an attribution requirement.
The source tree retains the original FreeType notices. Installation includes
`LICENSES/FTL.TXT` and `LICENSES/FreeType-third-party.txt`; `3RDPARTY.md` contains
the required credit. FontTools is a host build dependency; it is not linked
into the bootloader.

References: [FreeType licences](https://freetype.org/license.html),
[FreeType embedding instructions](https://github.com/freetype/freetype/blob/VER-2-14-3/docs/INSTALL.ANY),
[FreeType customisation](https://github.com/freetype/freetype/blob/VER-2-14-3/docs/CUSTOMIZE),
[FontTools variable font instancer](https://fonttools.readthedocs.io/en/latest/varLib/instancer.html).

## Verification

```sh
bash tests/ui/gui-host.sh
python3 tests/ui/gui_vm_test.py
python3 tests/ui/menu_vm_test.py --baseline work/fonts/before.BOOTX64.EFI
bash tests/ui/font-jump.sh
```

The font tests compare our actual compositor to direct FreeType rendering:
112 pixel-exact image and advance comparisons across both fonts, seven sizes
and two tracking values. They also exercise kerning, clipping, Unicode,
unsupported and malformed UTF-8, cache eviction, malformed SFNT tables and
repeatable recovery with an intentionally exhausted allocation budget.
The host suite compiles the same FreeType modules/configuration used in the
bootloader under AddressSanitizer and UndefinedBehaviourSanitizer.
QEMU checks cover GUI navigation, booting and fallback, including a valid theme
pack containing an invalid font. Screenshots and logs are under `work/fonts`.

The context-recovery test runs 128 nested-stack recoveries per architecture at
both `-O0` and `-O2` in QEMU user emulation. All six variants pass. The matching
UEFI targets compile; this does not establish full firmware boot or GUI runtime
coverage on those architectures. The script uses system `qemu-aarch64`,
`qemu-riscv64` and `qemu-loongarch64`, or the local copies in `work/tools/usr/bin`.
Compatibility-pass evidence is under `work/compatibility`.
