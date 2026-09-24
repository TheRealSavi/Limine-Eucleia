# Eucleia UI configuration

`eucleia.conf` controls the graphical menu. `limine.conf` continues to supply
boot entries, submenus, ordering, default selection, timeouts, protocols, editing
policy and the original terminal settings. Eucleia does not create boot entries.
This document describes the implemented authoring features; the broader
[GUI guide](GUI.md) describes the rendering implementation.

## Try it

From the repository root, after `./dev build`:

```sh
./dev restart --no-build
./dev restart --no-build --ui examples/minimal.conf
./dev screenshot
```

Edit `themes/classical/eucleia.conf` for the default artwork, or copy the minimal
example and adjust it. Restart stages the file and its referenced assets into a
private VM disk. `--no-build` reuses the existing loader and asset pack, so ordinary
UI edits do not require compilation. The running bootloader does not reload files.
A file-watching native preview remains a later milestone.

The launcher resolves asset sources beside the chosen host configuration and
preserves their paths in the guest. `icons/linux.png` means a host `icons/`
folder beside that configuration. An absolute guest path such as `/fonts/ui.ttf`
is staged from `fonts/ui.ttf` beside the host configuration, never from the
host's `/fonts` directory. The supplied `/boot/eucleia.eui` is taken from
`work/theme/eucleia.eui` unless the configuration directory supplies its own
`boot/eucleia.eui`. Parent traversal (`..`) is not supported by this staging
helper. Missing assets are reported and left for the guest's fallback handling.

## Discovery and fallback

Limine discovers its boot configuration as before. Eucleia looks for
`eucleia.conf` in that selected file's directory on the same boot volume. A
configuration at `/boot/limine/limine.conf` therefore uses
`/boot/limine/eucleia.conf`, even if another file exists at `/eucleia.conf`.
Discovery is case-insensitive like Limine's configuration discovery.

Asset paths are volume paths, not URIs. Relative paths resolve beside the
loaded `eucleia.conf`; leading `/` means the boot volume's root. Configurations
supplied through SMBIOS without a disk configuration path retain the terminal
interface. Quiet boot, serial output and unsupported framebuffer modes retain
existing fallback behaviour.

- Missing `eucleia.conf`, or `enabled: no`: original terminal menu.
- Invalid configuration, theme pack, required background, visible image header or font: terminal
  menu with an Eucleia diagnostic. A property error includes its source line.
- Missing or invalid per-entry icon: generic icon and a GUI diagnostic.
- Missing or invalid cursor PNG: built-in pointer and a GUI diagnostic.
- Unmatched entry selector: no effect; it never adds an entry.
- Runtime font failure: terminal fallback.

Fallback preserves Limine's timeout and boot policy. It does not introduce an
extra pause. The terminal editor and protocol/error screens are unchanged.
The earlier experimental `interface_renderer` and `eucleia_theme` keys in
`limine.conf` are no longer used. Move GUI settings to the companion file.

## Syntax and defaults

Use UTF-8 text, `key: value`, sections such as `[menu]`, and full-line `#`
comments. Names and values are case-sensitive. Spaces surrounding keys and
values are trimmed; inline comments and quoted values are not supported.
Root fields belong before the first section. Section names may repeat, but a
property may appear only once in each section. Unknown sections/properties,
invalid units and invalid geometry are errors. Colours use `#RRGGBB`.
Booleans are `yes` or `no`.

```text
version: 1
enabled: yes
theme: /boot/eucleia.eui
title: MY COMPUTER

[menu]
x: 92%
y: 50%
anchor: right-center
width: 48%
height: 48%
padding: 16px

[entry "Linux/CachyOS"]
icon: icons/cachyos.png
```

The supplied `.eui` asset pack remains required in this first implementation.
It provides fonts, default artwork and legacy layout defaults. Explicit
configuration values override those defaults. Normal and selected row surfaces
are configured independently; an omitted selected field keeps its pack default.
There are no configuration includes, variables or style inheritance yet.

Root fields:

| Key | Default and meaning |
| --- | --- |
| `version` | `1`; other versions are rejected |
| `enabled` | `yes`; `no` selects the terminal menu |
| `theme` | `/boot/eucleia.eui`; version 2 asset pack |
| `title` | `EUCLEIA`; heading text, which may be empty |

## Coordinates and dimensions

`px` means logical display pixels after rotation. `u` means reference units,
uniformly scaled to fit the pack's reference canvas (1920x1080 in the supplied
pack). Geometry uses that scale; font sizes retain a minimum scale of 2/3 for
readability. Explicit coordinates start at the logical display's top-left and
do not add the default layout's centring offset. Decimal values permit up to
three decimal places and resolve to whole pixels.

Percentages are accepted for screen-relative `x`, `y`, `width`, menu `height`,
menu width constraints, heading `height` and divider `height`. Horizontal values use screen
width; vertical values use screen height. Other lengths require `px` or `u`.
`letter_spacing` uses `px` and retains fractional spacing.

Menu width is clamped to its min/max constraints before applying the anchor.
The anchor places the corresponding point of the menu rectangle at `x`, `y`.
Supported anchors: `top-left`, `top-center`, `top-right`, `left-center`, `center`,
`right-center`, `bottom-left`, `bottom-center`, `bottom-right`.

The menu and its row contents must fit within the display and row bounds.
Invalid geometry falls back rather than silently moving it. Visible row count
comes from menu height, padding, row height and gap. Selection scrolls through
the same ordered entries from Limine. Text uses ellipsis and clips to its row;
there is no text wrapping or automatic row-height expansion. Authors must
leave sufficient vertical room for their chosen fonts and hint lines.

## Supported sections

| Section | Fields |
| --- | --- |
| `[menu]` | `x`, `y`, `width`, `height`, `min_width`, `max_width`, `anchor`, `padding` |
| `[menu.item]` | `height`, `gap`, `indent`, `icon_x`, `icon_size`, `indicator_x`, `indicator_size`, plus surface fields below |
| `[menu.item.label]` | `x`, `baseline`, `width`, `overflow: ellipsis`, plus text fields below |
| `[menu.item.selected]` | Surface fields, `text_colour`, `glow_colour`, `glow_width`, `glow_opacity`, `marker`, `marker_size`, `marker_colour` |
| `[heading]` | `mode`, `file`, `fit`, `x`, `y`, `width`, `height`, `visible`, plus text fields |
| `[hints]` | `layout`, `x`, `y`, `width`, `height`, `anchor`, `visible`, `padding`, `gap`, `item_height`, `columns`, `order`, `line_gap`, plus text and surface fields |
| `[hints.key]` | `colour`, `width`, `padding`, `gap`, `position`, plus surface fields |
| `[hints.ACTION]` | `visible`, `label`, `x`, `y`, `width`, `height` |
| `[details]` | `x`, `y`, `width`, `height`, `anchor`, `relative_to`, `visible`, `overflow`, `padding`, `line_gap`, plus text and surface fields |
| `[countdown]` | `x`, `y`, `width` |
| `[divider]` | `x`, `y`, `width`, `height`, `visible` |
| `[background]` | `visible`, `colour`, `file`, `fit` |
| `[cursor]` | `file`, `width`, `height`, `hotspot_x`, `hotspot_y` |
| `[entry "Full/Menu/Path"]` | `icon` |

Surface fields: `fill_colour`, `fill_opacity` (0..1), `border_colour`,
`border_width`, `corner_radius`. Borders are opaque. Radius is capped to half
the actual rectangle dimensions when drawing. `menu` is a viewport; it does
not yet have its own background surface.

Text fields: `font`, `font_size`, `colour`, `letter_spacing`, `align`.
Alignment is `left`, `center` or `right` within the configured text width.
`font` accepts a static TrueType file; omitting it uses the corresponding font
in the pack. Fonts render at their resolved pixel size. See [FONTS.md](FONTS.md)
for Unicode coverage, shaping limits and licensing.

Menu label `x` and `baseline` are relative to the row. Icons and indicators are
vertically centred; their x offsets are relative to the row. `indent` moves
nested labels, capped at four levels of visual indentation. Icon/indicator size
zero hides that image. Width changes automatically derive a default indicator
position and label width; explicit overrides take precedence.

Heading, hints, details and countdown positions are independent of the menu.
Their `y` values are text baselines by default. Image headings, structured hints,
and details with a positive `height` use the top of their box instead. Moving the
menu does not move these elements unless details use `relative_to: selection`.
Countdown uses the hints font and selected border colour, left-aligned. The
divider uses the pack's ornamental image.

Background `colour` fills the screen. `visible: no` hides wallpaper, leaving
that colour. `file` optionally replaces the pack wallpaper with a PNG.
`fit` is `cover` (default), `contain` or `stretch`; the image is centred.
PNG alpha blends over the configured background colour.

## Key assignment layouts

`[hints] layout` selects how key assignments are presented:

- `legacy` (default): the existing two lines and divider. `line_gap` is the
  baseline-to-baseline distance, default 32u. Existing configurations keep their
  appearance. Keycap, action and panel styling applies to structured layouts.
- `horizontal`: one line, with natural item widths when they fit. Otherwise
  items share the available width and text uses ellipsis.
- `vertical`: one item per row.
- `grid`: row-major cells with `columns: 1` through `6` (default 2).
- `custom`: each action has its own box inside the hints panel.

Structured hints use `x`, `y`, `width`, `height` and the same nine `anchor`
values as the menu. `padding` insets the panel contents; `gap` separates items
in automatic layouts; `item_height` sets their height. If omitted, panel height
reserves room for all six actions. Automatic layouts compact unavailable or
hidden actions. Custom boxes remain at their assigned positions. The panel
can use all surface fields for a fill, border and rounded corners.

The action names and actual keys are fixed:

| Action section | Key | Default label and availability |
| --- | --- | --- |
| `[hints.select]` | Arrows | Select, when entries exist |
| `[hints.enter]` | Enter | Boot, Expand or Collapse according to selection |
| `[hints.edit]` | E | Edit, when the editor is enabled and a boot entry is selected |
| `[hints.blank]` | B | Blank entry, when the editor is enabled |
| `[hints.firmware]` | S | Firmware, when firmware setup is available |
| `[hints.shell]` | U | Shell, when the firmware shell is available |

Set `visible: no` to hide an action or `label` to change its description. An
empty label shows only the key. Overriding Enter's label replaces its dynamic
Boot/Expand/Collapse description. These are presentation settings; they do not
remap keys or enable unavailable actions. Limine's help suppression still
hides the entire hints panel.

`order` is a comma-separated list of all six action names, each exactly once;
the default is `select, enter, edit, blank, firmware, shell`. In custom mode,
per-action `x`, `y`, `width` and `height` are relative to the panel's padded
content area, and percentages refer to that area. Automatic layouts use the
shared `item_height` and ignore per-action geometry.

`[hints.key]` styles the keycap independently from its label. `colour` sets key
text colour; text size and font come from `[hints]`. `width: 0px` (default)
fits each key naturally; a positive width fixes all keycap widths. Leave room
for the full key name at small resolutions, where fonts retain their minimum
scale. `padding` is horizontal space inside the keycap and `gap` separates it
from its label. `position` is `before` (default), `after` or `above`. Above
requires an item height of at least twice the font size plus the key gap.
Surface fields set keycap fill, border and corners; a transparent fill and
zero border leave plain text. `align` on `[hints]` aligns the group or each cell.

```ini
[hints]
layout: grid
x: 6%
y: 80%
width: 88%
height: 16%
columns: 3
item_height: 48u
gap: 14u
font_size: 24u
letter_spacing: 0px
align: left

[hints.key]
fill_colour: #284b5e
fill_opacity: 1
border_colour: #72c8e7
border_width: 1px
padding: 8u
gap: 14u

[hints.edit]
label: Edit options
```

## Selected-entry description

`[details]` displays the selected entry's `comment` from `limine.conf`. It now
accepts its own font, size, colour, tracking and alignment. Omitted text fields
inherit the hints style, except alignment which defaults to left. A matching
font file and pixel size share the hints font in memory.

With the default `height: 0px`, `y` remains a baseline and the description is a
single line with ellipsis. A positive height makes it a panel: `y` is the top,
`padding` insets the text, and surface fields provide a fill, border and corners.
`overflow: wrap` wraps at spaces, breaks long words at UTF-8 boundaries and adds
ellipsis to the last available line. It requires a positive panel height.
`line_gap` adds space between lines. Explicit newlines also start a new line.
An empty or missing comment hides the panel.

`relative_to: screen` (default) positions the box on the screen. With
`relative_to: selection`, `x` and `y` are offsets from the selected visible
row's top-left corner. Negative offsets are permitted and percentages still
refer to the screen. The same nine anchors are available. The box follows
selection and scrolling; any part outside the display is clipped. Position it
with enough clearance from the list, as there is no collision avoidance.

```ini
[details]
relative_to: selection
x: 50%
y: 0px
width: 33%
height: 150u
padding: 18u
font_size: 26u
colour: #bed9e5
fill_colour: #172631
fill_opacity: 1
overflow: wrap
line_gap: 6u
```

Runnable examples:

```sh
python3 dev.py restart --no-build --ui themes/classical/eucleia.conf
python3 dev.py restart --no-build --ui examples/hints-grid.conf
python3 dev.py restart --no-build --ui examples/hints-vertical.conf
python3 dev.py restart --no-build --ui examples/hints-custom.conf
```

Build first with `python3 dev.py build` after updating the renderer. These
examples change presentation while using the existing boot configuration.

## Text or PNG header

`[heading]` defaults to `mode: text`, rendering the root `title` with the
heading font, colour, alignment and tracking. `mode: image` instead draws
the PNG at `file` inside the configured `width` and positive `height`.
The image preserves its aspect ratio and is centred (`fit: contain`);
`fit: stretch` fills the entire box. Transparency reveals the wallpaper.
Image mode does not draw `title` on top of the PNG. Text fields remain available
for switching back to text mode. `visible: no` skips header image loading.

```text
[heading]
mode: image
file: components/eucleia-header.png
x: 208u
y: 26u
width: 720u
height: 360u
fit: contain

[divider]
visible: no
```

The supplied header contains the laurel, rules, wordmark and tagline together.
The separate divider is therefore hidden. To use live text instead, replace
the heading section with the following; `title` belongs at the file's root:

```text
[heading]
mode: text
x: 218u
y: 270u
width: 656u
font_size: 72u
colour: #d8cdbd
align: center
```

## Selected row decoration

The border and fill continue to be rendered at the actual row size. An optional
outer glow and diamond marker add the concept's selection treatment without
stretching a bitmap border or including the row label in an image.

```text
[menu.item.selected]
glow_width: 12u
glow_opacity: 0.24
glow_colour: #d5995c
marker: diamond-left
marker_size: 16u
marker_colour: #f2c392
```

`marker` accepts `none` (default), `diamond-left` or `diamond-right`. The
diamond is centred on the chosen border and the row's vertical midpoint.
`marker_size` is its full width/height, defaulting to `16u`; zero hides it.
`glow_width` defaults to zero; `glow_opacity` defaults to 0.25. Glow and marker
colours default to the selected border colour. The marker also receives the
configured glow. Leave room around the menu for decoration: it can extend
outside the row and is clipped at the screen edge. Text and icons still clip
to the row. Decoration does not enlarge its mouse hit area. The existing right
chevron remains an independent selected-entry/submenu indicator; its size can
be set to zero to hide it.

## Mouse cursor

The GUI accepts a transparent PNG cursor. Omit `file` to keep the built-in
pointer. Paths resolve beside `eucleia.conf`, like other direct PNG assets.

```text
[cursor]
file: components/cursor.png
width: 20px
height: 28px
hotspot_x: 2px
hotspot_y: 1px
```

`width` and `height` are the drawn dimensions, defaulting to 24px and 32px.
The image stretches into that box; use its aspect ratio to avoid distortion.
The hotspot is the actual click position, measured from the image's top-left
in drawn pixels, including transparent margins. Both hotspot offsets default
to zero and must lie inside the drawn box. These dimensions accept `px` or `u`;
use `px` for a fixed cursor size across resolutions.

Cursor PNGs may be at most 256x256; each drawn dimension must resolve to
1..128px. Their decoded pixels count towards the shared 16 MiB PNG budget.
The compositor clips the cursor at screen edges, honours display rotation,
and restores the old pixels on movement. A missing or invalid PNG restores the
16x23 built-in pointer with a zero hotspot. The terminal editor retains its
existing pointer behaviour.

## Icons and entry names

```text
[entry "CachyOS"]
icon: icons/cachyos.png

[entry "Snapshots/2026-09-24"]
icon: icons/snapshots.png

[entry "Linux#1"]
icon: icons/linux.png
```

Selectors match full menu paths, including submenus. Escape literal `/`, `#`
and `\` in a name with `\`, following Limine's `default_entry` convention.
`#0` identifies the first same-named sibling and can be omitted; `#1` is the
second. Only entries available for the current firmware/architecture count.
Renaming or moving an entry requires updating its selector. Reordering duplicate
names can change which entry the suffix identifies. Matching supports up to
64 path components. A future authoring tool will list selectors and diagnose
unmatched assignments; the current runtime only applies exact matches.

PNG icons retain their colours and transparency and are scaled to `icon_size`.
Repeated references to the identical path share one decoded resource.
The Classical theme supplies 32 icons, including Windows, macOS, Linux
distributions and boot tools. See the [catalogue and assignment examples](../themes/classical/icons/README.md)
in source packages, or `theme/icons/README.md` in binary packages. Copy the
needed assignments from `icons/entries.conf` into your configuration and match
your actual menu paths; icons are not assigned by operating-system detection.

## Limits and current scope

- Configuration: 32 KiB, 256 properties, 32 icon assignments; value length 1024
  bytes. Resolved asset paths must fit within 511 bytes.
- PNG file: 4 MiB encoded; icon/cursor dimensions at most 256x256; heading dimensions
  at most 2048x2048; background dimensions at most 4096 per axis.
  Total additional decoded images: 16 MiB, including the header.
- Theme pack: 16 MiB. External fonts: 2 MiB per face, four font roles (details shares hints when possible).
  Font arenas retain the existing bounded allocation policy.
- Resolved font size: 4..256px; tracking: -16..64px; row height: 16..512px;
  gap/padding/indent: 0..256px; border: 0..16px; radius: 0..128px.
- Hint labels: 128 bytes; item height: up to 512px; key width: 0..512px;
  key padding/gap: 0..128px. Wrapped descriptions examine at most 4096 bytes,
  draw at most 64 lines, and limit each line to 1024 bytes.
- Selection glow: 0..32px; diamond: 0..128px, no taller than the row when enabled.
- New decoded images, font file buffers and a countdown backing rectangle add
  to the existing GUI memory footprint. The included configurations are tested
  at 1280x720 with 256 MiB; arbitrary maximal assets need more memory.

This supports one vertical menu with an independent text or image header and
supporting text. Additional decorative elements, named styles, responsive variants,
independent countdown typography, configurable default icon assets,
asset-pack-free themes and a file-watching native preview remain planned work.
