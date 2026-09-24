#include <ui/theme.h>

static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8
        | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool span(size_t offset, size_t length, size_t size, size_t start) {
    return offset >= start && offset <= size && length <= size - offset;
}

bool ui_theme_read(struct ui_theme *theme, const void *data, size_t size) {
    const uint8_t *p = data;
    const char magic[] = "EUCTHM02";
    if (size < 32 + UI_ASSET_COUNT * 32 + UI_LAYOUT_COUNT * 4
     || size > UI_THEME_MAX_SIZE || ((uintptr_t)data & 3) != 0) {
        return false;
    }
    for (size_t i = 0; i < 8; i++) {
        if (p[i] != (uint8_t)magic[i]) {
            return false;
        }
    }
    size_t table_end = 32 + UI_ASSET_COUNT * 32;
    size_t layout = word(p + 20);
    if (word(p + 8) != 2 || word(p + 12) != size
     || word(p + 16) != UI_ASSET_COUNT || layout != table_end
     || word(p + 24) != 0 || word(p + 28) != 0) {
        return false;
    }
    struct ui_theme result = {0};
    size_t start = table_end + UI_LAYOUT_COUNT * 4;
    for (size_t i = 0; i < UI_ASSET_COUNT; i++) {
        const uint8_t *record = p + 32 + i * 32;
        uint32_t kind = word(record), id = word(record + 4);
        uint32_t width = word(record + 8), height = word(record + 12);
        size_t offset = word(record + 16), length = word(record + 20);
        size_t glyph_offset = word(record + 24), count = word(record + 28);
        bool font = i >= UI_HEADING && i <= UI_HINT;
        if (id != i || kind != (font ? 3u : 1u) || (offset & 3) != 0
         || !span(offset, length, size, start) || count != 0 || glyph_offset != 0) {
            return false;
        }
        if (font) {
            if (width < 4 || width > 128 || height != 0
             || length < 12 || length > 2 * 1024 * 1024) {
                return false;
            }
        } else if (width == 0 || width > 4096 || height == 0 || height > 2160
                || length != (uint64_t)width * height * 4) {
            return false;
        }
        result.assets[i] = (struct ui_asset){.kind = kind, .width = width, .height = height,
            .pixels = p + offset, .size = length};
    }
    for (size_t i = 0; i < UI_LAYOUT_COUNT; i++) {
        result.layout[i] = word(p + layout + i * 4);
        if (i < UI_TEXT && result.layout[i] > 4096) {
            return false;
        }
    }
    const uint32_t *l = result.layout;
    if (l[UI_REF_W] < 640 || l[UI_REF_H] < 480 || l[UI_REF_H] > 2160
     || l[UI_PANEL_W] < 200 || l[UI_PANEL_X] + l[UI_PANEL_W] > l[UI_REF_W]
     || l[UI_ROW_H] < 32 || l[UI_MAX_ROWS] == 0 || l[UI_MAX_ROWS] > 20
     || l[UI_MENU_Y] + l[UI_MAX_ROWS] * (l[UI_ROW_H] + l[UI_ROW_GAP]) > l[UI_FOOTER_Y]
     || l[UI_FOOTER_Y] + l[UI_FOOTER_BASELINE] >= l[UI_REF_H]
     || l[UI_TITLE_Y] >= l[UI_MENU_Y] || l[UI_SELECTED_ALPHA] > 255
     || l[UI_ICON_SIZE] == 0 || l[UI_CHEVRON_SIZE] == 0
     || l[UI_DIVIDER_W] == 0 || l[UI_DIVIDER_H] == 0
     || l[UI_HEADING_TRACK] > 4096 || l[UI_LABEL_TRACK] > 4096
     || l[UI_HINT_TRACK] > 4096 || l[UI_FOOTER_BASELINE] > 256
     || l[UI_STATUS_Y] >= l[UI_REF_H] || l[UI_LABEL_MAX] > l[UI_PANEL_W]) {
        return false;
    }
    *theme = result;
    return true;
}
