# Eucleia graphical menu

The boot menu now has a native framebuffer renderer in Limine-owned code.
It draws the prepared artwork, proportional antialiased text, icons, translucent
selection borders, optional glow/edge diamonds, a text or PNG header, scrolling
rows, comments, countdown and keyboard hints.
The menu controller still decides selection, expansion, editing and booting.

## Run it

The development config enables the GUI. From the repository root:

```sh
./dev restart
./dev screenshot
```

`./dev build` also builds the theme pack from the supplied artwork and fonts.
`./dev run` and `./dev restart` copy it into the private VM disk. Every default
entry remains a test payload or a chainload of the VM's own EFI binary.

The graphical appearance is configured in `themes/classical/eucleia.conf`. The loader
reads this companion beside the selected `limine.conf`; boot entries and policy
stay in Limine's original file. Missing or disabled UI configuration selects the
terminal menu. See [CONFIGURATION.md](CONFIGURATION.md) for the supported schema,
asset paths, fallback rules and examples.

```sh
./dev restart --no-build --ui examples/minimal.conf
```

## Implementation map

| File | Responsibility |
| --- | --- |
| `common/menu_gui.c` | Framebuffer renderer, layout, selection decoration, text roles, scrolling, countdown, hit testing and GUI pointer |
| `common/ui/display.c` | Physical framebuffer validation, rotated presentation and pointer coordinate conversion |
| `common/ui/canvas.c` | Clipped software drawing, alpha blending, bilinear image sampling and native-size glyph compositing, text measurement and UTF-8 decoding |
| `common/ui/support.c` | Keycap layouts and wrapped selected-entry descriptions |
| `common/ui/font.c` | FreeType memory faces, hinting, glyph cache, kerning and bounded font arenas |
| `common/ui/theme.c` | Bounded pack validation |
| `common/ui/config.c` | UI document parsing, validation and resolved geometry/styles |
| `common/ui/assets.c` | Bounded file reads and direct PNG loading |
| `common/drivers/mouse.c` | Switchable pixel/terminal coordinate spaces, device conversion and press/release positions |
| `common/menu.c` | Renderer selection and controller actions; both click edges must hit the same entry |
| `tools/build-theme.py` | Host-side font instancing, PNG conversion and layout tokens into the runtime pack |

The GUI writes its own RGB canvas directly to the primary framebuffer and
honours its pitch, RGB/BGR channel layout and `interface_rotation`. The primary
display is the first framebuffer selected by Limine. Additional displays are
blank while the GUI is active; editing and errors retain existing terminal
output behaviour. The GUI does not draw through terminal
characters or use terminal rows for placement. Existing graphics initialisation
still selects the display mode and keeps a terminal ready for editing, errors
and fallback. The downloaded Flanterm sources are unchanged.

The renderer caches the background and fixed text/PNG heading/divider. Navigation copies
that canvas and draws live rows. Pointer-only changes restore the old pointer
rectangle, then draw the new pointer. `[cursor]` can supply a transparent PNG,
drawn dimensions and click hotspot; omitting it retains the built-in arrow.
Leaving the GUI restores terminal
coordinates and terminal output; drawing again resumes the GUI after editing.

## Change the theme

Ordinary layout, typography, colours, title, wallpaper and per-entry icon edits
belong in `eucleia.conf` and need no pack rebuild. The pack supplies base assets
and defaults. It currently uses the assets and tokens in `themes/classical`.
Make a copy of `tokens.json`, edit panel positions, spacing, type sizes, tracking
or palette, then build it without modifying the example theme:

```sh
python3 tools/build-theme.py \
    --tokens work/my-theme.json \
    --output work/theme/my-theme.eui
```

`--kit` can select another directory with the same asset layout. The builder uses
the background, ornamental divider, boot-entry and submenu icons, Cinzel heading
font and EB Garamond label/hint font. Row borders, glow, diamond markers and
selection fill are drawn dynamically; row labels are actual configuration
entries. The token file's required `background` path selects the packed
wallpaper, relative to the kit. The default configuration loads its transparent
header separately from `components/eucleia-header.png`.

An optional `runtime` object in the token file controls `max_rows` (default 5),
`footer_baseline` (default 26) and `status_y` (default 880). These use the same
reference canvas as the other layout values. The companion's `[hints]` section
controls the Classical theme's keycap footer.

Layout scales uniformly inside the screen; the wallpaper covers it. Small modes
retain at least two-thirds of the reference font sizes for readability. Long
labels use an ellipsis and the selected entry stays inside the list viewport.

The renderer supports one vertical list with an independently placed text/PNG heading,
details, countdown and hints. Key hints support horizontal, vertical, grid and
individual placements. Description panels have independent typography, wrapping
and optional positioning relative to selection. Per-entry PNGs and external static TrueType fonts
can be loaded directly. More general decorative elements and native live
preview remain follow-up work; see the configuration reference for exact scope.

## Font and pack contract

FreeType rasterises font outlines inside the bootloader at the final pixel size.
Light auto-hinting, native grayscale masks and a common baseline avoid distorted
text at fractional display scales. The compositor uses proportional advances,
kerning, tracking and clipped ellipses. See [FONTS.md](FONTS.md) for configuration,
licensing, implementation and typography limits.

The host builder selects the requested weight from variable fonts using
FontTools. Font source files remain unchanged. Unicode coverage comes from the
font; full contextual shaping and bidirectional text are not implemented.

The generated pack is about 7.3 MiB and uses little-endian fields:

| Region | Format |
| --- | --- |
| Header, 32 bytes | `EUCTHM02`, version 2, total size, 8 assets, layout offset, two reserved zero words |
| Asset table, 8 x 32 bytes | Kind, ID, width, height, data offset/length, two reserved zero words |
| Layout, 32 x 4 bytes | Token indices declared in `common/ui/theme.h` |
| Image data | Kind 1: BGRA pixels; width and height are image dimensions |
| Font data | Kind 3: TrueType bytes; width is reference font size, height is zero |

Asset IDs are background, heading font, label font, hint font, divider, boot icon,
submenu icon and chevron. All data sections have four-byte alignment. Rebuild
version 1 packs with `./dev build`; they otherwise select the terminal fallback.
Retain the kit's font source/licence files when distributing derived packs.

## Limits and fallback

- A primary 32-bit RGB/BGR framebuffer with 0, 90, 180 or 270 degree rotation.
  Landscape and portrait modes are accepted: each dimension must be between
  600 and 4096, at least one must be 800 or greater, and the total pixel count
  must not exceed 4096x2160. Unsupported modes, text/serial output or invalid
  assets use the terminal renderer.
- Theme file limit: 16 MiB. Image bounds: 4096x2160. Font files: 2 MiB
  per face. Glyph masks: 256x256.
- GUI memory is the pack, two `width * height * 4` canvases and three 4 MiB
  font arenas (four when details needs a different font or size), plus direct PNG/font overrides and a countdown backing rectangle.
  With the supplied PNG header, at 2560x1440 that is about 54 MiB, in addition
  to the existing terminal and bootloader. At the maximum supported size it is
  about 93 MiB with the supplied pack and header.
- Assets remain allocated while editing so the GUI can resume. Existing menu
  rewind/reinitialisation reclaims them after a failed boot.
- The fallback editor and protocol/error screens retain their existing terminal
  appearance. Boot policy, security policy and protocol loading remain in the
  existing controller/protocol code.

## Verification

```sh
bash tests/ui/gui-host.sh
python3 tests/ui/authoring_vm_test.py
python3 tests/ui/theme_vm_test.py
python3 tests/ui/gui_vm_test.py
python3 tests/ui/mouse_vm_test.py
python3 tests/ui/compat_vm_test.py
```

The host tests run the real parser, compositor, GUI adapter and UEFI pointer
driver with simulated firmware input under AddressSanitizer and
UndefinedBehaviourSanitizer. They cover malformed/truncated assets, proportional
metrics, UTF-8 fallback, clipping, alpha blending, framebuffer pitch padding,
RGB/BGR modes, all quarter-turn rotations, portrait modes, scrolled hit testing,
pointer bounds, input flushing and editor
leave/resume. Synthetic absolute-pointer events verify conversion to pixels and
back to terminal cells, including press/release origins.

All 23 QEMU GUI fixtures passed: five display sizes, navigation,
submenus, long scrolling lists, editor cancel/modified boot, timeout cancellation,
automatic/immediate/quiet boot, quiet reveal, nested defaults, empty menus,
failed-boot/editor recovery and missing/invalid/oversized-asset or invalid-font
fallback. Late glyph failure also switches to a functioning terminal menu.
Small display fallback screens match the terminal reference. The terminal
regression suite also passed with all 33 frames unchanged.

The live development menu also successfully chainloaded its own EFI binary and
returned to the same GUI frame.

The separate compatibility suite exercises rotation, portrait output, multiple
displays, SeaBIOS runtime, EFI chainloading and Linux boot. All five upstream
UEFI architecture targets compile. See [COMPATIBILITY.md](COMPATIBILITY.md) for
the exact runtime matrix, memory checks and remaining validation limits.

OVMF mouse delivery is verified through QEMU's VirtIO tablet and mouse devices.
The installed firmware lacks PS/2 and USB mouse drivers; the development launcher
now supplies the supported VirtIO tablet. Ten pointer integration fixtures cover
hover, submenu clicks and boot at three sizes, editor return, drag rejection, timeout
cancellation, failed-boot recovery, terminal fallback and unmodified upstream.
Relative VirtIO mouse movement is also checked. See [COMPATIBILITY.md](COMPATIBILITY.md)
for the source research and device matrix. Physical hardware remains outside
this VM test coverage.
No host EFI files or host firmware variables were changed.
