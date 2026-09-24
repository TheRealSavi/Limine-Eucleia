#ifndef UI__CONFIG_H__
#define UI__CONFIG_H__

#include <ui/theme.h>

#define UI_CONFIG_MAX_SIZE 32768
#define UI_CONFIG_MAX_FIELDS 256
#define UI_CONFIG_MAX_ICONS 32
#define UI_HINT_COUNT 6

enum ui_hint_layout { UI_HINT_LEGACY, UI_HINT_HORIZONTAL, UI_HINT_VERTICAL, UI_HINT_GRID, UI_HINT_CUSTOM };
enum ui_hint_action { UI_HINT_SELECT, UI_HINT_ENTER, UI_HINT_EDIT, UI_HINT_BLANK, UI_HINT_FIRMWARE, UI_HINT_SHELL };

struct ui_property {
    char *section, *key, *value;
    unsigned line;
    bool used;
};

struct ui_document {
    struct ui_property fields[UI_CONFIG_MAX_FIELDS];
    size_t count;
    unsigned error_line;
    const char *error;
};

struct ui_rect { int x, y, width, height; };
struct ui_text_style {
    const char *file;
    int size, tracking, align;
    uint32_t colour;
};
struct ui_surface {
    uint32_t fill, border;
    int opacity, border_width, radius;
};
struct ui_icon_rule { const char *entry, *file; };
struct ui_hint_item {
    const char *label;
    bool visible;
    struct ui_rect box;
};
struct ui_settings {
    const char *title, *background_file, *heading_file, *cursor_file;
    bool heading_visible, divider_visible, hints_visible, details_visible, wallpaper;
    bool heading_image, heading_stretch;
    int background_fit;
    uint32_t background_colour, selected_colour;
    struct ui_rect menu, heading, divider, hints, details, countdown;
    int padding, row_height, row_gap, indent, icon_x, icon_size;
    int label_x, label_baseline, label_width, chevron_x, chevron_size;
    int hint_gap, max_rows;
    enum ui_hint_layout hint_layout;
    int hint_padding, hint_item_gap, hint_item_height, hint_columns, hint_order[UI_HINT_COUNT];
    int key_padding, key_gap, key_width, key_position;
    uint32_t key_colour;
    struct ui_surface hint_panel, key_surface, detail_panel;
    struct ui_hint_item hint_items[UI_HINT_COUNT];
    int detail_padding, detail_line_gap;
    bool detail_wrap, detail_selection;
    int cursor_width, cursor_height, cursor_hotspot_x, cursor_hotspot_y;
    int glow_width, glow_opacity, marker_side, marker_size;
    uint32_t glow_colour, marker_colour;
    struct ui_text_style text[4];
    struct ui_surface rows[2];
    struct ui_icon_rule icons[UI_CONFIG_MAX_ICONS];
    size_t icon_count;
};

// The document borrows and splits a writable, NUL-terminated input of size bytes.
bool ui_document_read(struct ui_document *doc, char *data, size_t size);
const char *ui_document_get(struct ui_document *doc, const char *section, const char *key);
bool ui_document_enabled(struct ui_document *doc, bool *enabled);
bool ui_settings_read(struct ui_settings *settings, struct ui_document *doc,
    const struct ui_theme *theme, int width, int height);
bool ui_config_path(char *out, size_t capacity, const char *config, const char *asset);

#endif
