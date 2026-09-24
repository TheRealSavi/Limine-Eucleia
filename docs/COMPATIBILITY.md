# Compatibility and validation

Limine-Eucleia targets the existing Limine platforms. The original terminal
renderer handles unsupported graphical modes and unavailable or invalid UI
configuration. Editing and boot-error screens retain terminal rendering.

## Validated coverage

| Platform or path | Coverage |
| --- | --- |
| x86-64 UEFI / OVMF | GUI, keyboard, VirtIO mouse/tablet, editing and payload boot |
| BIOS / SeaBIOS | Q35 and PC, PS/2 input, GUI, editor return and payload boot |
| Displays | Standard VGA, Bochs display, portrait and all quarter-turn rotations |
| Multiple displays | GUI on Limine's first framebuffer; other outputs blank during GUI use |
| Memory | Supplied 1280x720 theme and test payload at 256 MiB under UEFI and BIOS; normal fixtures use 512 MiB |
| Existing protocols | Limine test payload, EFI chainloading/load options, Linux into a minimal initramfs |
| UEFI architecture builds | IA32, x86-64, AArch64, RISC-V64 and LoongArch64 |
| Font recovery adapters | AArch64, RISC-V64 and LoongArch64 under user emulation |

The configuration milestone passed 14 authoring fixtures, 23 GUI fixtures,
10 real-pointer fixtures and 19 compatibility fixtures. All 33 compared
terminal frames matched the pre-configuration build. Host ASan/UBSan checks
include 112 native FreeType pixel/advance comparisons. Reproduce these checks
with `tests/ui/README.md` in the source distribution; generated evidence stays in
`work/` rather than the source release.

The recorded runtime environment was CachyOS with QEMU 11.1.1, OVMF 202608,
SeaBIOS 1.17 and Linux kernels 6.18.52 LTS and 7.2.6. Firmware contents matter:
OVMF pointer support depends on its included drivers, not merely the QEMU input
device. The test guest uses a VirtIO tablet supported by that firmware.

## Runtime bounds

The GUI needs a primary 32-bit RGB/BGR framebuffer. Each logical dimension is
600..4096, at least one is 800 or greater, and total pixels are at most
4096x2160. Configuration/assets and fonts have additional bounds documented in
[CONFIGURATION.md](CONFIGURATION.md) and [FONTS.md](FONTS.md).

The BIOS linker enforces `bss_end <= 0x96000`, leaving conventional RAM for
BIOS I/O. Configuration/composition and font code use size optimisation on BIOS.
Total system RAM does not remove this conventional-memory constraint.

Physical machines and non-x86/IA32 firmware boots have not been covered by the
maintained VM tests. Compilation is not a runtime guarantee. Secure Boot/BLI
variables, firmware setup and remaining protocols need dedicated integration
coverage. Unicode follows the chosen fonts; contextual shaping, bidirectional
layout and font-family fallback are not implemented. Memory checks describe
the supplied themes and test payload, not arbitrary themes or operating systems.
