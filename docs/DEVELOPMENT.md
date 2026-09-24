# Development

Run commands from the repository root. The normal launcher builds an x86-64
UEFI loader, theme and protocol test payload, then starts QEMU with a private
128 MiB FAT disk, private OVMF variables, 512 MiB RAM and no network adapter.
KVM is used if available; otherwise the launcher uses TCG. It requires no root
access and does not read or modify a host EFI partition.

## Dependencies

Build tools are listed in `../INSTALL.md`. The development workflow additionally
needs Python 3.9 or newer, Pillow, FontTools, QEMU x86 system emulation, mtools
and a matching pair of OVMF firmware files. A graphical session needs QEMU's
GTK display module. The installed Arch/CachyOS firmware paths are the defaults;
set `OVMF_CODE` and `OVMF_VARS` to use another matching pair.

```sh
./dev check
./dev run
./dev restart
./dev restart --no-build --ui examples/minimal.conf
./dev screenshot
./dev key down
./dev status
./dev stop
./dev run --headless
```

`run` and `restart` build incrementally. `--no-build` reuses existing binaries
and the theme pack while staging edited configuration/assets. Edit
`themes/classical/eucleia.conf` for the default UI and `examples/limine.conf`
for the private guest's boot entries. Linux, Windows, Recovery and Tools labels
demonstrate the shipped icon assignments. These examples use only the bundled
test kernel and a chainload of the guest's own loader. The deployment template
at `themes/classical/limine.conf` is separate and needs machine-specific paths.

The launcher supplies a VirtIO tablet for OVMF pointer input. Firmware must
include its VirtIO input driver. The tested Arch firmware has this driver;
PS/2 and USB mouse support depends on the firmware build. BIOS tests separately
exercise PS/2 input. See [compatibility](COMPATIBILITY.md).

On Arch/CachyOS, `python3 tools/setup-display.py` can install matching GTK QEMU
modules into ignored `work/tools/` without root. It checks package hashes and
signatures and never changes the system package database. Other distributions
should use their normal QEMU GTK package. This helper is optional.

## Editor checks

The checked-in VS Code C/C++ settings match the freestanding include paths and
defines in `common/common.mk`. Select `UEFI x86-64`, `BIOS i686`, or `Host tests`
with `C/C++: Select IntelliSense Configuration`. Bootstrap the dependencies and
configure the corresponding build directory first. If using different build
directories, adjust the generated-header paths in `.vscode/c_cpp_properties.json`.

The UEFI configuration reads `compile_commands.json` when present, so individual
host tests can use their own compiler flags. With Bear installed, refresh it
after changing build options or adding sources:

```sh
bear --output compile_commands.json -- make -C work/build -B -j8
bear --append --output compile_commands.json -- bash tests/ui/gui-host.sh
```

Python checks are configured in `pyproject.toml`. Use the same interpreter with
Pillow and FontTools installed in the editor and on the command line. The local
`typings/fontTools` stubs describe only the APIs used here; extend them when
adding calls to FontTools. With Pyright, Mypy, Ruff and rumdl installed, run:

```sh
pyright
mypy
ruff check dev.py tools tests typings
rumdl check docs/CONFIGURATION.md docs/GUI.md tests/ui/README.md
```

## Generated files

`work/build` contains the loader build, `work/theme` the compiled pack, `work/vm`
the running VM files, and `work/logs` the logs. `work/` is ignored and is not a
release input. Regression tests put their generated files beneath it as well.
See [tests/ui/README.md](../tests/ui/README.md) for host and firmware checks.

The launcher can pause at reset with `./dev restart --debug`. Its GDB socket is
`work/vm/gdb.sock`. Loading relocated EFI symbols requires the actual firmware
load address. Closing QEMU's window stops the guest.

For a clean development build, stop the VM and remove only `work/build`.
Deleting `work/vm/OVMF_VARS.fd` resets that guest's firmware state. Bootstrap
fetches pinned dependencies and resets their checkouts; keep project adapters
in `common/ui/freetype` and do not edit the fetched FreeType sources.
