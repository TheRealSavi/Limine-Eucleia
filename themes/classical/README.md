# Classical

A complete example theme for Limine-Eucleia: a coastal statue wallpaper,
ivory Roman lettering, bronze selection borders, live entry labels and a
compact keycap footer. The layout scales from a 1920x1080 design canvas.

## Try it

From a source checkout, run `./dev run` at the repository root. The private
QEMU guest uses `examples/limine.conf` with Linux, Windows, Recovery and Tools
labels to demonstrate the theme. All demonstration payloads boot the bundled
test kernel; the guest does not access installed operating systems or host disks.

## Use it

Keep your existing working boot entries. Place `eucleia.conf`, `components/`
and `icons/` beside the `limine.conf` loaded by Limine. Place the compiled pack
at `/boot/eucleia.eui` on the boot volume, or update `theme:` in `eucleia.conf`.
A binary package supplies the pack at `theme/boot/eucleia.eui`.

The source `limine.conf` (packaged as `limine.conf.example`) shows matching
Linux, Windows, Recovery and Tools entries. It is a template: replace the root
UUID and file paths for your system before using it. The EFI entries require
UEFI and their named executables. Existing entries need only matching icon
selectors in `eucleia.conf`, including parent submenu names.

The [icon reference](icons/README.md) lists all 32 icons, and
[entries.conf](icons/entries.conf) has assignments to copy. Unassigned entries
use generic boot or submenu icons.

## Customise it

Edit `eucleia.conf` for colours, layout, font sizes, header, cursor, selection
and footer. These changes do not require a pack rebuild. `u` values scale with
the canvas; `px` values retain their physical size. Boot behaviour and entry
names belong in `limine.conf`.

The pack contains the wallpaper, divider, generic navigation icons and three
font faces. The header and cursor are transparent PNGs loaded separately from
`components/`; assigned entry icons are loaded from `icons/`.
`tokens.json` supplies the compiler's base geometry, palette and font settings.
It is required when building from source and is not deployed to the boot volume.

From the source repository root, rebuild after editing pack inputs:

```sh
python3 tools/build-theme.py
./dev restart --no-build
```

For a configured release build, `make theme` writes `theme/eucleia.eui` inside
that build directory. See the repository's `docs/CONFIGURATION.md` for all
supported properties and `docs/DEPLOYMENT.md` for deployment.

## Sources and notices

[Source provenance](SOURCES.md) records the generated artwork, pinned font
inputs and operating-system icon sources. Preserve it, the
[icon notices](icons/README.md), the [icon licence](icons/LICENSE) and both
font licences in `fonts/` when redistributing. The project code licence is in
the repository's `COPYING`; the font licences and Gentoo icon licence also
apply to their derivatives.
