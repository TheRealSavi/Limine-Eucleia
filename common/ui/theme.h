#ifndef UI__THEME_H__
#define UI__THEME_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_THEME_MAX_SIZE (16 * 1024 * 1024)
#define UI_ASSET_COUNT 8
#define UI_LAYOUT_COUNT 32

enum {
    UI_BACKGROUND, UI_HEADING, UI_LABEL, UI_HINT, UI_DIVIDER,
    UI_BOOT_ICON, UI_FOLDER_ICON, UI_CHEVRON
};

enum {
    UI_REF_W, UI_REF_H, UI_PANEL_X, UI_PANEL_W, UI_TITLE_Y, UI_MENU_Y,
    UI_ROW_H, UI_ROW_GAP, UI_ICON_X, UI_ICON_SIZE, UI_LABEL_X, UI_LABEL_BASELINE,
    UI_CHEVRON_X, UI_CHEVRON_SIZE, UI_DIVIDER_X, UI_DIVIDER_Y, UI_DIVIDER_W,
    UI_DIVIDER_H, UI_FOOTER_Y, UI_LABEL_MAX, UI_TEXT, UI_SELECTED_TEXT, UI_BRONZE,
    UI_MUTED, UI_INK, UI_SELECTED_ALPHA, UI_HEADING_TRACK, UI_LABEL_TRACK,
    UI_HINT_TRACK, UI_MAX_ROWS, UI_FOOTER_BASELINE, UI_STATUS_Y
};

struct ui_asset {
    uint32_t kind, width, height;
    const uint8_t *pixels;
    size_t size;
};

struct ui_theme {
    struct ui_asset assets[UI_ASSET_COUNT];
    uint32_t layout[UI_LAYOUT_COUNT];
};

// The validated theme borrows its storage. The pack uses little-endian words.
bool ui_theme_read(struct ui_theme *theme, const void *data, size_t size);

#endif
