#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p work/gui work/fonts
python3 tools/build-theme.py
python3 - <<'PY'
from pathlib import Path
import struct
from PIL import Image
Image.new('RGBA', (4, 2), (240, 160, 80, 128)).save('work/gui/header.png')
pack = Path('work/theme/eucleia.eui').read_bytes()
for role, index in [('heading', 1), ('label', 2)]:
    offset, size = struct.unpack_from('<2I', pack, 32 + index * 32 + 16)
    Path(f'work/fonts/{role}.ttf').write_bytes(pack[offset:offset+size])
PY
source tests/ui/font-host-build.sh
cc "${ft_flags[@]}" -DBIOS -DCOM_OUTPUT=0 -DFLANTERM_IN_FLANTERM \
    -Dfopen=gui_test_fopen -Dfread=gui_test_fread -Dfclose=gui_test_fclose \
    -Icommon -Iflanterm/src tests/ui/gui_host_test.c \
    common/ui/theme.c common/ui/canvas.c common/ui/display.c common/ui/font.c common/menu_gui.c \
    common/ui/config.c common/ui/support.c common/ui/assets.c common/menu_model.c common/lib/stb_image.c \
    work/fonts/host/libfreetype.a \
    -o work/gui/host-test
work/gui/host-test work/theme/eucleia.eui work/gui/header.png
cc "${ft_flags[@]}" tests/ui/ui_config_test.c common/ui/config.c common/ui/theme.c \
    -o work/gui/config-test
work/gui/config-test work/theme/eucleia.eui
cc "${ft_flags[@]}" tests/ui/support_host_test.c common/ui/font.c common/ui/canvas.c \
    common/ui/support.c common/ui/config.c common/ui/theme.c work/fonts/host/libfreetype.a \
    -o work/gui/support-test
work/gui/support-test work/theme/eucleia.eui
cc "${ft_flags[@]}" tests/ui/font_host_test.c common/ui/font.c common/ui/canvas.c \
    work/fonts/host/libfreetype.a -o work/fonts/host/font-test
work/fonts/host/font-test
cc "${ft_flags[@]}" -DUI_FONT_BUDGET=4096 tests/ui/font_budget_test.c common/ui/font.c \
    work/fonts/host/libfreetype.a -o work/fonts/host/budget-test
work/fonts/host/budget-test
cc -std=gnu11 -ffreestanding -Wall -Wextra -Werror -g -O1 \
    -fsanitize=address,undefined -DUEFI -DCOM_OUTPUT=0 -DFLANTERM_IN_FLANTERM \
    -Icommon -Iflanterm/src -Ipicoefi/inc \
    tests/ui/mouse_canvas_test.c common/drivers/mouse.c \
    -o work/gui/mouse-test
work/gui/mouse-test

cc -std=gnu11 -ffreestanding -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -Icommon tests/ui/menu_model_test.c common/menu_model.c common/menu_renderer.c \
    -o work/gui/model-test
work/gui/model-test
cc -std=gnu11 -ffreestanding -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -DBIOS -DCOM_OUTPUT=0 -DFLANTERM_IN_FLANTERM -Icommon -Iflanterm/src \
    tests/ui/menu_terminal_test.c common/menu_terminal.c -o work/gui/terminal-test
work/gui/terminal-test
