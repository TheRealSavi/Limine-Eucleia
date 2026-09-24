# Using Limine-Eucleia

This is a Limine-Eucleia package. Its firmware loaders retain Limine's boot
protocols and configuration. See `USAGE.md` for deployment of the appropriate
BIOS or UEFI loader and `CONFIG.md` for boot entries. The portable `limine`
host utility can be built with `make`; firmware images are already compiled
in a binary package. The package manifest lists which targets are included.

## Enable the graphical menu

The binary package includes a `theme/` directory:

- Place `theme/eucleia.conf` beside the `limine.conf` actually loaded.
- Place `theme/icons/` and `theme/components/` beside that `eucleia.conf`.
- Place `theme/boot/eucleia.eui` at `/boot/eucleia.eui` on the boot volume,
  or change `theme:` in the companion to the path you use.
- Retain the theme's notices and font licences when redistributing it.

The Classical example pairs Linux, Windows, Recovery and Tools entries with
matching icons. Keep your working `limine.conf` when applying the theme; adjust
the icon selectors in `eucleia.conf` to match your entry names, including parent
submenu names. Unassigned entries use generic icons.

`theme/limine.conf.example` is an optional starting point for new configurations.
Replace `YOUR-ROOT-UUID`, kernel/initramfs paths and EFI paths before using it.
Its EFI entries require UEFI and the named executables on the selected volume.
Remove entries for software you do not have. Source checkouts contain this
template at `themes/classical/limine.conf`. Keep boot entries, default selection,
timeouts and protocols in `limine.conf`; display mode and rotation retain their
original Limine settings.

Set `enabled: no` in `eucleia.conf`, or omit the companion, to use the terminal
menu. Invalid required UI data also falls back with a diagnostic. The terminal
editor and error screens are retained.

See `docs/CONFIGURATION.md` for the actual supported properties and relative
asset-path rules. `examples/minimal.conf` demonstrates another composition.
A theme pack is currently required even when overriding its artwork/fonts.

The package does not automatically write to an EFI partition, modify firmware
variables, enrol signing keys or replace boot entries. Files can first be tested
in the isolated QEMU workflow described in the source distribution.
