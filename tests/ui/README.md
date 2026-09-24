# UI and menu regression checks

For the framebuffer GUI, run `bash tests/ui/gui-host.sh` and
`python3 tests/ui/gui_vm_test.py` after `./dev build`. See [GUI guide](../../docs/GUI.md)
for coverage and supported modes. The following checks cover the retained
terminal renderer and the shared model.

Run commands from the repository root. The host checks require a C compiler with
AddressSanitizer and UndefinedBehaviourSanitizer. The VM comparison uses the
existing development tools, Python Pillow and KVM. Firmware defaults match
`./dev`; override them with `OVMF_CODE` and `OVMF_VARS` on other hosts.
Baseline binaries and generated evidence are local test inputs, not shipped
files. Supply `--baseline` or `--upstream` when using a fresh checkout.

## Host checks

```sh
mkdir -p work/menu-refactor
cc -std=c11 -ffreestanding -Wall -Wextra -Werror \
    -fsanitize=address,undefined -g -Icommon \
    tests/ui/menu_model_test.c common/menu_model.c common/menu_renderer.c \
    -o work/menu-refactor/menu-model-test
work/menu-refactor/menu-model-test
cc -std=gnu11 -ffreestanding -Wall -Wextra -Werror \
    -fsanitize=address,undefined -g \
    -DBIOS -DCOM_OUTPUT=0 -DFLANTERM_IN_FLANTERM -Icommon -Iflanterm/src \
    tests/ui/menu_terminal_test.c common/menu_terminal.c \
    -o work/menu-refactor/menu-terminal-test
work/menu-refactor/menu-terminal-test
```

This runs the real model and renderer selector with a host allocator and test
renderers. It covers filtering, collapsed and nested trees, stable visible
indices, bounds, replacing a same-size tree, releasing allocations, and preferred
renderer success/failure/default fallback.

The terminal checks use the real renderer with a terminal sink and mouse output
stubs. They exercise centred and scrolled hit testing, empty/outside positions,
presentation, leaving the menu and resuming after editing. They do not exercise
firmware input events or rasterisation.

## QEMU comparison

Save a known-good EFI build **before** changing source. For the initial extraction
that copy is `work/menu-refactor/baseline.BOOTX64.EFI`, built from upstream
`34fe53c3d27d229ea25c28fc3e04db362543715b`.

```sh
./dev build
python3 tests/ui/menu_vm_test.py \
    --baseline work/menu-refactor/baseline.BOOTX64.EFI
```

`--candidate` optionally selects another EFI binary; the default is
`work/build/bin/BOOTX64.EFI`. Each fixture gets a new private OVMF variable store
and read-only FAT disk containing only the loader, bundled test kernel,
wallpaper and fixture config. VMs have no network. Test disks are removed after
each fixture; screenshots, configs and logs remain in `work/menu-refactor/`.

The comparison exercises selection wrapping, Home/End, nested expansion,
scrolling and long names, numeric boot, editor cancel/modified boot, countdown
cancellation, automatic/immediate/quiet boot, nested default paths, empty/missing
config, custom colours, invalid and directory defaults, firmware terminal
fallback, quiet cancellation and failed-boot return to menu/editor. Payload boots
must produce the expected command line and success marker in debugcon output.

Framebuffer comparisons are exact RGB comparisons of stable menu/editor screens.
The script writes `work/menu-refactor/vm-results.json` and fails on differences
so they can be inspected. Countdown timing can depend on host load; compare the
actual remaining value before treating such a difference as a rendering change.

One intentional difference is checked separately: an empty menu revealed from
quiet mode now renders the complete frame, including branding and a hidden text
cursor. It must exactly match the normal empty-menu screen. The baseline omitted
the branding and retained a text cursor on that first frame.

Mouse delivery is covered separately by the VirtIO pointer suite below.
The compatibility suite below also covers native EFI chainloading and Linux.
Firmware setup/reboot, the UEFI shell, Secure Boot/BLI variables and other boot
protocols still need their own fixtures.

## OVMF pointer integration

```sh
python3 tests/ui/mouse_vm_test.py
```

This uses the actual OVMF VirtIO input driver and QMP hardware events. Ten
fixtures cover GUI hover/click/submenu/boot at three sizes, editor return,
drag rejection, timeout cancellation, failed-boot recovery, terminal fallback,
unmodified upstream and relative mouse movement. The default upstream binary
is the saved baseline above; override it with `--upstream`. Evidence is saved
under `work/mouse-research/integration/`. See [compatibility guide](../../docs/COMPATIBILITY.md)
for tested firmware requirements. Wheel scrolling is not delivered by the tested
OVMF pointer driver.

## Compatibility runtime and architecture builds

Build the normal UEFI loader/theme with `./dev build`, then build BIOS and the
isolated EFI/Linux payloads:

```sh
(
    mkdir -p work/menu-refactor/build-bios
    cd work/menu-refactor/build-bios
    ../../../configure --enable-bios --disable-uefi-x86-64 --disable-uefi-cd
    make -j16
)
python3 tests/ui/build-compat-fixtures.py
python3 tests/ui/compat_vm_test.py
```

The default suite has 19 fixtures with the two installed Linux kernels. It
covers portrait output, three rotations, primary-only multiple-display output,
standard VGA and Bochs display, SeaBIOS on Q35 and PC, PS/2 interaction, editor
return, 256 MiB GUI operation under both firmware types, EFI load options and
Linux entry into a minimal initramfs. The other fixtures use 512 MiB. Optional
`--suite displays|bios|memory|boot` runs one group. Each additional Linux kernel
adds two fixtures; use repeatable `--kernel /path/to/vmlinuz` to choose kernels.
The default copies installed `/usr/lib/modules/*/vmlinuz` files read-only.

The BIOS harness creates a private MBR/FAT32 image and runs the built Limine
installer against that regular file. Its IDE drive uses `snapshot=on` so guest
writes are disposable. UEFI fixtures use private variables and read-only disk
images. There are no host devices or networks attached. Configurations, QEMU
commands, screenshots, logs and results remain under `work/compatibility/vm`;
disk images are removed after each fixture.

Compile the other existing UEFI targets separately:

```sh
(
    mkdir -p work/compatibility/build-ports
    cd work/compatibility/build-ports
    ../../../configure --disable-bios --disable-uefi-cd \
        --enable-uefi-ia32 --enable-uefi-aarch64 \
        --enable-uefi-riscv64 --enable-uefi-loongarch64
    make -j16
)
bash tests/ui/font-jump.sh
```

The recovery test requires Clang/LLD and QEMU user emulators for AArch64,
RISC-V64 and LoongArch64, from the system or `work/tools/usr/bin`. It checks
nested-stack recovery at two optimisation levels using the bootloader's
integer-only ABIs. Architecture builds and adapter tests do not establish
full boot or GUI coverage for those firmware targets. See
[COMPATIBILITY.md](../../docs/COMPATIBILITY.md) for results and remaining limits.

Font-specific checks are included in `gui-host.sh`: native FreeType pixel comparisons,
kerning, UTF-8, cache eviction, malformed font tables and memory-budget recovery.
See [font rendering](../../docs/FONTS.md) for the rendering and dependency contract.

## Companion configuration

```sh
python3 tests/ui/authoring_vm_test.py
```

This uses the current x86-64 UEFI/BIOS builds and theme pack. Fixtures cover
absent/disabled configuration, the default and a different composition,
mouse selection after repositioning, editor return, PNG backgrounds and
positioned countdown cancellation, relative icons/fonts beside
a nested Limine configuration, malformed UI settings, missing assets, 256 MiB
operation and Limine-owned automatic boot. Images and logs are saved under
`work/authoring/vm`. Host parser/geometry/path checks run in `gui-host.sh`.
The model test also checks nested, escaped and duplicate entry selectors.

## Classical example theme

```sh
python3 tests/ui/theme_vm_test.py
```

This boots each demonstration entry, including the Tools submenu, and captures
`work/theme/vm/classical.png` for visual review. It also checks the shipped PNG
header and diamond/glow configuration at 800x600,
1280x720, 1920x1080, 2560x1440 and portrait 720x1280 under UEFI, plus 1280x720
under BIOS. Each uses 256 MiB and boots the isolated test payload. It verifies
selection restoration, cached header stability, editor return, both diamond
sides and UEFI submenu mouse input. Screenshots and results are written to
`work/theme/vm`. Build the BIOS loader as described above before running it.
The host suite also checks header alpha/contain/stretch, clipped decoration,
unchanged row hit areas and failed/hidden header image loading and cleanup.
Cursor checks cover PNG alpha blending, maximum cursor size, hotspot clipping
at every corner and rotation, pixel restoration and invalid-image fallback.
The theme VM fixture also checks the real cursor's bounds and movement before
using it to open a submenu and boot its child.

## Key help and selected-entry descriptions

`bash tests/ui/gui-host.sh` checks structured hint layout, availability, keycap
borders, wrapping, final-line ellipsis, clipping and independent font cleanup
under AddressSanitizer and UndefinedBehaviorSanitizer.

`python3 tests/ui/hints_vm_test.py` exercises the four example layouts, directory
hint changes, description updates, editor return and booting, plus 800x600,
1920x1080 and BIOS at 256 MiB. Captures are in `work/hints/vm`.
