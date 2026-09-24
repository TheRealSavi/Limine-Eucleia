#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p work/compatibility/jump
for arch in aarch64 riscv64 loongarch64; do
    case "$arch" in
        aarch64) flags=(-march=armv8-a+nofp+nosimd -mgeneral-regs-only) ;;
        riscv64) flags=(-march=rv64imac -mabi=lp64 -mno-relax) ;;
        loongarch64) flags=(-march=loongarch64 -mabi=lp64s -mfpu=none -msimd=none -mno-relax) ;;
    esac
    emulator="$(command -v "qemu-$arch" || true)"
    if [[ -z "$emulator" ]]; then
        emulator="work/tools/usr/bin/qemu-$arch"
    fi
    for optimisation in 0 2; do
        binary="work/compatibility/jump/$arch-O$optimisation"
        clang --target="$arch-unknown-linux-gnu" "${flags[@]}" -std=gnu11 \
            -O"$optimisation" -ffreestanding -nostdlib -nostdinc -static -fuse-ld=lld \
            -fno-stack-protector -fno-omit-frame-pointer -Icommon \
            -isystem freestanding-c-hdrs/include tests/ui/font_jump_test.c \
            common/ui/freetype/jump.S -Wl,-e,_start -o "$binary"
        "$emulator" "$binary"
        echo "Passed: $arch -O$optimisation, 128 nested-stack recoveries"
    done
done
