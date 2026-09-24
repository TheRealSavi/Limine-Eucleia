# Classical icons

32 transparent PNG icons in the theme's warm bronze (`#e0b88f`). The four
navigation icons are 48x48; the others are 128x128. All fit the runtime's
256x256 icon limit.

## Assign an icon

Add the matching section to your `eucleia.conf`. The name must match the entry
in `limine.conf`, including its submenu path:

```ini
[entry "Windows"]
icon: icons/windows.png

[entry "CachyOS"]
icon: icons/cachyos.png

[entry "Tools/UEFI Shell"]
icon: icons/terminal.png
```

Copy only the assignments you need from [`entries.conf`](entries.conf), and
adjust their entry names. These are presentation settings; they do not create
boot entries or enable the operations represented by the icons. Assignment is
explicit: the renderer does not identify operating systems from their names.
Keep the PNGs beside `eucleia.conf` in an `icons/` directory. `dev.py` copies
assigned PNGs into the VM automatically.

`[menu.item] icon_size` controls the displayed size. The Classical theme uses
`40u`, which becomes approximately 27 pixels at 1280x720 and 40 pixels at
1920x1080. Transparency
lets the wallpaper and selected row show through.

## Included icons

| System | PNG | System | PNG |
| --- | --- | --- | --- |
| Windows | `windows.png` | macOS | `macos.png` |
| Linux (Tux) | `linux.png` | Arch Linux | `arch.png` |
| CachyOS | `cachyos.png` | Ubuntu | `ubuntu.png` |
| Debian | `debian.png` | Fedora | `fedora.png` |
| Linux Mint | `mint.png` | Manjaro | `manjaro.png` |
| openSUSE | `opensuse.png` | NixOS | `nixos.png` |
| EndeavourOS | `endeavouros.png` | Pop!_OS | `popos.png` |
| FreeBSD | `freebsd.png` | Gentoo | `gentoo.png` |

| Use | PNG | Use | PNG |
| --- | --- | --- | --- |
| Recovery | `recovery.png` | USB boot | `usb.png` |
| Internal drive | `disk.png` | Network boot | `network.png` |
| UEFI shell / terminal | `terminal.png` | Tools | `tools.png` |
| Secure boot | `secure-boot.png` | Memory test | `memory-test.png` |
| Restart | `restart.png` | Shut down | `shutdown.png` |
| Submenu | `folder.png` | Optical disc | `optical-disc.png` |
| Snapshots | `snapshots.png` | Firmware settings | `firmware.png` |
| Generic boot entry | `boot-enter.png` | Expand indicator | `chevron-right.png` |

## Sources and notices

The Windows symbol and boot/navigation symbols are original project vector
geometry covered by the repository's BSD-2-Clause `COPYING`.

The other operating-system outlines are adapted from
[font-logos](https://github.com/Lukas-W/font-logos/tree/d3bf5d299e54595db1b19681a0cc57ab10454857),
at commit `d3bf5d299e54595db1b19681a0cc57ab10454857`. The upstream
[Unlicense](LICENSE) is included. [Source links](../SOURCES.md) identify each
original outline. Adaptations apply bronze colouring, a common canvas and
inset, then export transparent PNGs.
The original names, marks and logos belong to their respective owners and are
used here to identify boot entries. No affiliation or endorsement is implied.

The Gentoo source carries a separate
[Creative Commons Attribution-ShareAlike 2.5 licence](https://creativecommons.org/licenses/by-sa/2.5/).
Its title is "Gentoo Logo Dark v1.0" (2011-06-03), by Sebastian Pipping
(`sping@gentoo.org`), based on Lennart Andre Rolland's original vector version;
rights are credited to Lennart Andre Rolland and Gentoo Foundation Inc.
The original metadata also credits an unknown contributor for the dark-version
idea and RGB value. `gentoo.png` is a recoloured, resized adaptation of that
work and remains under CC-BY-SA-2.5. Preserve this attribution and the source
notices when redistributing the icon set.
