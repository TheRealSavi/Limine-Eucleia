#!/usr/bin/env bash
# Sourced by host tests so the tested library uses the bootloader configuration.
mkdir -p work/fonts/host
ft_flags=(-std=gnu11 -ffreestanding -Wall -Wextra -Werror -g -O1
    -fsanitize=address,undefined -Icommon/ui/freetype -Ifreetype/include -Icommon)
mapfile -t ft_sources < <(python3 - <<'PY'
from pathlib import Path
import re
print('\n'.join(re.findall(r'freetype/[^\s]+\.c', Path('common/ui/freetype/sources.mk').read_text())))
PY
)
ft_objects=()
for source in "${ft_sources[@]}"; do
    object="work/fonts/host/$(basename "${source%.c}").o"
    cc "${ft_flags[@]}" -DFT2_BUILD_LIBRARY -Wno-unused-variable -c "$source" -o "$object"
    ft_objects+=("$object")
done
ar rcs work/fonts/host/libfreetype.a "${ft_objects[@]}"
