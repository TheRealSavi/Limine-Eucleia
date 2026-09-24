#include <ui/config.h>
#include <lib/libc.h>

static char *trim(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    size_t n = strlen(p);
    while (n != 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r')) {
        p[--n] = 0;
    }
    return p;
}

static bool error(struct ui_document *doc, const char *message) {
    if (doc->error == NULL) {
        doc->error = message;
    }
    return false;
}

bool ui_document_read(struct ui_document *doc, char *data, size_t size) {
    memset(doc, 0, sizeof(*doc));
    if (size == 0 || size > UI_CONFIG_MAX_SIZE) {
        return error(doc, "empty or oversized configuration");
    }
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (c == 0 || (c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 127) {
            return error(doc, "invalid control character");
        }
    }
    char *section = "";
    unsigned line = 0;
    for (char *next = data; next != NULL;) {
        char *p = next;
        next = strchr(p, '\n');
        if (next != NULL) {
            *next++ = 0;
        }
        doc->error_line = ++line;
        p = trim(p);
        if (*p == 0 || *p == '#') {
            continue;
        }
        if (*p == '[') {
            size_t len = strlen(p);
            if (len < 3 || p[len - 1] != ']') {
                return error(doc, "malformed section");
            }
            p[len - 1] = 0;
            section = trim(p + 1);
            const char *names[] = {"menu", "menu.item", "menu.item.label", "menu.item.selected",
                "heading", "hints", "hints.key", "hints.select", "hints.enter", "hints.edit",
                "hints.blank", "hints.firmware", "hints.shell", "details", "countdown", "divider", "background", "cursor"};
            bool valid = false;
            for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
                valid |= strcmp(section, names[i]) == 0;
            }
            len = strlen(section);
            if (len > 8 && strncmp(section, "entry \"", 7) == 0 && section[len - 1] == '"') {
                valid = true;
            }
            if (!valid) {
                return error(doc, "unknown section");
            }
            continue;
        }
        char *value = strchr(p, ':');
        if (value == NULL || doc->count == UI_CONFIG_MAX_FIELDS) {
            return error(doc, "expected key: value or too many fields");
        }
        *value++ = 0;
        char *key = trim(p);
        value = trim(value);
        if (*key == 0 || strlen(key) > 48 || strlen(value) > 1024) {
            return error(doc, "invalid key or oversized value");
        }
        for (size_t i = 0; i < doc->count; i++) {
            if (strcmp(doc->fields[i].section, section) == 0 && strcmp(doc->fields[i].key, key) == 0) {
                return error(doc, "duplicate property");
            }
        }
        doc->fields[doc->count++] = (struct ui_property){section, key, value, line, false};
    }
    doc->error_line = 0;
    return true;
}

const char *ui_document_get(struct ui_document *doc, const char *section, const char *key) {
    for (size_t i = 0; i < doc->count; i++) {
        struct ui_property *p = &doc->fields[i];
        if (strcmp(p->section, section) == 0 && strcmp(p->key, key) == 0) {
            p->used = true;
            if (doc->error == NULL) {
                doc->error_line = p->line;
            }
            return p->value;
        }
    }
    return NULL;
}

static void boolean(struct ui_document *doc, const char *section, const char *key, bool *result) {
    const char *v = ui_document_get(doc, section, key);
    if (v == NULL) {
        return;
    }
    if (strcmp(v, "yes") == 0) {
        *result = true;
    } else if (strcmp(v, "no") == 0) {
        *result = false;
    } else {
        error(doc, "expected yes or no");
    }
}

bool ui_document_enabled(struct ui_document *doc, bool *enabled) {
    *enabled = true;
    boolean(doc, "", "enabled", enabled);
    return doc->error == NULL;
}

static bool number(const char **text, int *value) {
    const char *p = *text;
    bool negative = *p == '-';
    if (negative) {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return false;
    }
    int n = 0;
    while (*p >= '0' && *p <= '9') {
        n = n * 10 + *p++ - '0';
        if (n > 32768) {
            return false;
        }
    }
    n *= 1000;
    if (*p == '.') {
        p++;
        int place = 100;
        if (*p < '0' || *p > '9') {
            return false;
        }
        while (*p >= '0' && *p <= '9' && place != 0) {
            n += (*p++ - '0') * place;
            place /= 10;
        }
    }
    *value = negative ? -n : n;
    *text = p;
    return true;
}

static int dimension(struct ui_document *doc, const char *section, const char *key,
    int fallback, int parent, int scale, bool percent) {
    const char *v = ui_document_get(doc, section, key);
    if (v == NULL) {
        return fallback;
    }
    int n;
    if (!number(&v, &n)) {
        error(doc, "invalid dimension");
        return fallback;
    }
    int64_t result;
    if (strcmp(v, "px") == 0) {
        result = n;
    } else if (strcmp(v, "u") == 0) {
        result = (int64_t)n * scale / 65536;
    } else if (percent && strcmp(v, "%") == 0 && n >= 0 && n <= 100000) {
        result = (int64_t)n * parent / 100;
    } else {
        error(doc, "expected px, u or permitted % dimension");
        return fallback;
    }
    if (result < -32768000 || result > 32768000) {
        error(doc, "dimension out of range");
        return fallback;
    }
    return (result + (result < 0 ? -500 : 500)) / 1000;
}

static void colour(struct ui_document *doc, const char *section, const char *key, uint32_t *out) {
    const char *v = ui_document_get(doc, section, key);
    if (v == NULL) {
        return;
    }
    if (strlen(v) != 7 || *v++ != '#') {
        error(doc, "expected #RRGGBB colour");
        return;
    }
    uint32_t n = 0;
    for (size_t i = 0; i < 6; i++) {
        unsigned c = v[i];
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c -= 'a' - 10;
        } else if (c >= 'A' && c <= 'F') {
            c -= 'A' - 10;
        } else {
            error(doc, "invalid colour digit");
            return;
        }
        n = (n << 4) | c;
    }
    *out = n;
}

static void text_style(struct ui_document *doc, struct ui_text_style *t, const char *section, int scale) {
    const char *file = ui_document_get(doc, section, "font");
    if (file != NULL) { t->file = file; }
    t->size = dimension(doc, section, "font_size", t->size, 0, scale, false);
    colour(doc, section, "colour", &t->colour);
    const char *v = ui_document_get(doc, section, "letter_spacing");
    if (v != NULL) {
        int n;
        if (!number(&v, &n) || strcmp(v, "px") != 0 || n < -16000 || n > 64000) {
            error(doc, "letter_spacing must be -16px to 64px");
        } else {
            t->tracking = (int64_t)n * 64 / 1000;
        }
    }
    v = ui_document_get(doc, section, "align");
    if (v != NULL) {
        if (strcmp(v, "left") == 0) {
            t->align = 0;
        } else if (strcmp(v, "center") == 0) {
            t->align = 1;
        } else if (strcmp(v, "right") == 0) {
            t->align = 2;
        } else {
            error(doc, "expected left, center or right alignment");
        }
    }
    if (t->size < 4 || t->size > 256) {
        error(doc, "font_size must resolve to 4..256px");
    }
}

static void rect(struct ui_document *doc, struct ui_rect *r, const char *section,
    int width, int height, int scale, bool sized) {
    r->x = dimension(doc, section, "x", r->x, width, scale, true);
    r->y = dimension(doc, section, "y", r->y, height, scale, true);
    r->width = dimension(doc, section, "width", r->width, width, scale, true);
    if (sized) {
        r->height = dimension(doc, section, "height", r->height, height, scale, true);
    }
}

static void opacity(struct ui_document *doc, const char *section, const char *key, int *out) {
    const char *v = ui_document_get(doc, section, key);
    if (v != NULL) {
        int n;
        if (!number(&v, &n) || *v != 0 || n < 0 || n > 1000) {
            error(doc, "opacity must be 0..1");
        } else {
            *out = (n * 255 + 500) / 1000;
        }
    }
}

static void surface(struct ui_document *doc, struct ui_surface *s, const char *section, int scale) {
    colour(doc, section, "fill_colour", &s->fill);
    colour(doc, section, "border_colour", &s->border);
    s->border_width = dimension(doc, section, "border_width", s->border_width, 0, scale, false);
    s->radius = dimension(doc, section, "corner_radius", s->radius, 0, scale, false);
    opacity(doc, section, "fill_opacity", &s->opacity);
    if (s->border_width < 0 || s->border_width > 16 || s->radius < 0 || s->radius > 128) {
        error(doc, "border or radius out of range");
    }
}

static int scaled(int value, int scale) {
    return ((int64_t)value * scale + 32768) / 65536;
}

static int choice(struct ui_document *doc, const char *section, const char *key,
    int fallback, const char *const *names, size_t count) {
    const char *v = ui_document_get(doc, section, key);
    if (v == NULL) { return fallback; }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(v, names[i]) == 0) { return i; }
    }
    error(doc, "unknown option");
    return fallback;
}

static void anchor(struct ui_document *doc, struct ui_rect *box, const char *section) {
    const char *names[] = {"top-left", "top-center", "top-right", "left-center", "center",
        "right-center", "bottom-left", "bottom-center", "bottom-right"};
    int at = choice(doc, section, "anchor", 0, names, 9);
    box->x -= (at % 3) * box->width / 2;
    box->y -= (at / 3) * box->height / 2;
}

static void support_settings(struct ui_settings *s, struct ui_document *doc,
    int width, int height, int scale, int ts) {
    const char *layouts[] = {"legacy", "horizontal", "vertical", "grid", "custom"};
    s->hint_layout = choice(doc, "hints", "layout", UI_HINT_LEGACY, layouts, 5);
    s->hint_columns = 2;
    const char *v = ui_document_get(doc, "hints", "columns");
    if (v != NULL) {
        if (strlen(v) != 1 || *v < '1' || *v > '6') { error(doc, "columns must be 1..6"); }
        else { s->hint_columns = *v - '0'; }
    }
    s->hint_gap = dimension(doc, "hints", "line_gap", s->hint_gap, 0, scale, false);
    s->hint_padding = dimension(doc, "hints", "padding", 0, 0, scale, false);
    s->hint_item_gap = dimension(doc, "hints", "gap", scaled(16, scale), 0, scale, false);
    s->hint_item_height = dimension(doc, "hints", "item_height", s->text[2].size + scaled(16, scale), 0, scale, false);
    int rows = s->hint_layout == UI_HINT_HORIZONTAL ? 1 : UI_HINT_COUNT;
    if (s->hint_layout == UI_HINT_GRID) { rows = (UI_HINT_COUNT + s->hint_columns - 1) / s->hint_columns; }
    if (s->hint_layout != UI_HINT_LEGACY) {
        s->hints.height = 2 * s->hint_padding + rows * s->hint_item_height + (rows - 1) * s->hint_item_gap;
    }
    rect(doc, &s->hints, "hints", width, height, scale, true);
    anchor(doc, &s->hints, "hints");
    surface(doc, &s->hint_panel, "hints", scale);
    s->key_colour = s->text[2].colour;
    colour(doc, "hints.key", "colour", &s->key_colour);
    s->key_padding = dimension(doc, "hints.key", "padding", scaled(8, scale), 0, scale, false);
    s->key_gap = dimension(doc, "hints.key", "gap", scaled(10, scale), 0, scale, false);
    s->key_width = dimension(doc, "hints.key", "width", 0, 0, scale, false);
    const char *positions[] = {"before", "after", "above"};
    s->key_position = choice(doc, "hints.key", "position", 0, positions, 3);
    surface(doc, &s->key_surface, "hints.key", scale);
    const char *actions[] = {"select", "enter", "edit", "blank", "firmware", "shell"};
    v = ui_document_get(doc, "hints", "order");
    unsigned seen = 0;
    for (int i = 0; i < UI_HINT_COUNT; i++) {
        int action = i;
        if (v != NULL) {
            while (*v == ' ' || *v == '\t') { v++; }
            const char *end = v;
            while (*end != 0 && *end != ',' && *end != ' ' && *end != '\t') { end++; }
            action = -1;
            for (int j = 0; j < UI_HINT_COUNT; j++) {
                if ((size_t)(end - v) == strlen(actions[j]) && strncmp(v, actions[j], end - v) == 0) { action = j; }
            }
            v = end;
            while (*v == ' ' || *v == '\t') { v++; }
            if (action < 0 || (seen & (1u << action)) != 0 || (i < UI_HINT_COUNT - 1 ? *v != ',' : *v != 0)) {
                error(doc, "order must list all six actions once, separated by commas");
                break;
            }
            seen |= 1u << action;
            if (*v == ',') { v++; }
        }
        s->hint_order[i] = action;
    }
    int inner = s->hints.width - 2 * s->hint_padding;
    int inner_height = s->hints.height - 2 * s->hint_padding;
    int visible = 0;
    int minimum_height = s->key_position == 2 ? 2 * s->text[2].size + s->key_gap : s->text[2].size;
    for (int i = 0; i < UI_HINT_COUNT; i++) {
        char section[32] = "hints.";
        strcpy(section + 6, actions[i]);
        struct ui_hint_item *item = &s->hint_items[i];
        item->visible = true;
        boolean(doc, section, "visible", &item->visible);
        visible += item->visible;
        item->label = ui_document_get(doc, section, "label");
        if (item->label != NULL && strlen(item->label) > 128) { error(doc, "hint label exceeds 128 bytes"); }
        item->box = (struct ui_rect){0, i * (s->hint_item_height + s->hint_item_gap), inner, s->hint_item_height};
        rect(doc, &item->box, section, inner, inner_height, scale, true);
        if (s->hint_layout == UI_HINT_CUSTOM && item->visible
         && (item->box.x < 0 || item->box.y < 0 || item->box.width < 1 || item->box.height < minimum_height
          || item->box.x + item->box.width > inner || item->box.y + item->box.height > inner_height)) {
            error(doc, "custom hint does not fit inside hints panel");
        }
    }
    int columns = s->hint_layout == UI_HINT_HORIZONTAL ? visible : (s->hint_layout == UI_HINT_GRID ? s->hint_columns : 1);
    int needed_rows = visible != 0 && columns > 0 ? (visible + columns - 1) / columns : 0;
    if (s->hint_layout != UI_HINT_LEGACY && s->hint_layout != UI_HINT_CUSTOM && visible != 0
     && (inner - (columns - 1) * s->hint_item_gap < columns * (2 * s->key_padding + 1)
      || needed_rows * s->hint_item_height + (needed_rows - 1) * s->hint_item_gap > inner_height)) {
        error(doc, "hint layout does not fit inside panel");
    }
    if (s->hint_padding < 0 || s->hint_padding > 256 || s->hint_item_gap < 0 || s->hint_item_gap > 256
     || s->hint_gap < 0 || s->hint_gap > 512 || s->hint_item_height < minimum_height || s->hint_item_height > 512
     || s->key_padding < 0 || s->key_padding > 128 || s->key_gap < 0 || s->key_gap > 128
     || s->key_width < 0 || s->key_width > 512 || (s->key_width != 0 && s->key_width <= 2 * s->key_padding)
     || (s->hint_layout != UI_HINT_LEGACY && (inner < 1 || inner_height < s->hint_item_height))) {
        error(doc, "hint spacing or geometry out of range");
    }
    s->text[3] = s->text[2];
    s->text[3].align = 0;
    text_style(doc, &s->text[3], "details", ts);
    const char *relative[] = {"screen", "selection"};
    s->detail_selection = choice(doc, "details", "relative_to", 0, relative, 2) == 1;
    if (s->detail_selection) { s->details.x = s->details.y = 0; }
    rect(doc, &s->details, "details", width, height, scale, true);
    anchor(doc, &s->details, "details");
    const char *overflows[] = {"ellipsis", "wrap"};
    s->detail_wrap = choice(doc, "details", "overflow", 0, overflows, 2) == 1;
    s->detail_padding = dimension(doc, "details", "padding", 0, 0, scale, false);
    s->detail_line_gap = dimension(doc, "details", "line_gap", scaled(6, scale), 0, scale, false);
    surface(doc, &s->detail_panel, "details", scale);
    if (s->detail_padding < 0 || s->detail_padding > 256 || s->detail_line_gap < 0 || s->detail_line_gap > 256
     || s->details.width <= 2 * s->detail_padding || s->details.width > width
     || s->details.height < 0 || s->details.height > height
     || (s->detail_wrap && s->details.height == 0)
     || (s->details.height > 0 && s->details.height < 2 * s->detail_padding + s->text[3].size)) {
        error(doc, "details panel or text does not fit");
    }
}

bool ui_settings_read(struct ui_settings *s, struct ui_document *doc,
    const struct ui_theme *theme, int width, int height) {
    memset(s, 0, sizeof(*s));
    const uint32_t *l = theme->layout;
    int scale = ((int64_t)width << 16) / l[UI_REF_W];
    int sy = ((int64_t)height << 16) / l[UI_REF_H];
    if (sy < scale) {
        scale = sy;
    }
    int ts = scale < 43691 ? 43691 : scale;
    int ox = (width - scaled(l[UI_REF_W], scale)) / 2;
    int oy = (height - scaled(l[UI_REF_H], scale)) / 2;
#define S(index) scaled(l[index], scale)
    s->title = "EUCLEIA";
    s->menu = (struct ui_rect){ox + S(UI_PANEL_X), oy + S(UI_MENU_Y), S(UI_PANEL_W),
        scaled(l[UI_MAX_ROWS] * (l[UI_ROW_H] + l[UI_ROW_GAP]) - l[UI_ROW_GAP], scale)};
    s->heading = (struct ui_rect){s->menu.x, oy + S(UI_TITLE_Y), s->menu.width, 0};
    s->divider = (struct ui_rect){ox + S(UI_DIVIDER_X), oy + S(UI_DIVIDER_Y), S(UI_DIVIDER_W), S(UI_DIVIDER_H)};
    s->hints = (struct ui_rect){s->menu.x, oy + S(UI_FOOTER_Y) + S(UI_FOOTER_BASELINE), s->menu.width, 0};
    s->details = (struct ui_rect){s->menu.x, oy + S(UI_STATUS_Y), s->menu.width, 0};
    s->countdown = s->details;
    s->countdown.y += scaled(34, scale);
    s->row_height = S(UI_ROW_H);
    s->row_gap = scaled(l[UI_ROW_H] + l[UI_ROW_GAP], scale) - s->row_height;
    s->menu.height = l[UI_MAX_ROWS] * (s->row_height + s->row_gap) - s->row_gap;
    s->icon_x = S(UI_ICON_X); s->icon_size = S(UI_ICON_SIZE);
    s->label_x = S(UI_LABEL_X); s->label_baseline = S(UI_LABEL_BASELINE);
    s->label_width = S(UI_LABEL_MAX); s->indent = scaled(20, scale);
    s->chevron_x = S(UI_CHEVRON_X); s->chevron_size = S(UI_CHEVRON_SIZE);
    s->hint_gap = scaled(32, scale);
    s->wallpaper = s->heading_visible = s->divider_visible = s->hints_visible = s->details_visible = true;
    s->background_colour = l[UI_INK];
    s->selected_colour = l[UI_SELECTED_TEXT];
    s->rows[1] = (struct ui_surface){l[UI_BRONZE], l[UI_BRONZE], l[UI_SELECTED_ALPHA], scaled(2, scale), 0};
    if (s->rows[1].border_width == 0) {
        s->rows[1].border_width = 1;
    }
    for (size_t i = 0; i < 3; i++) {
        int size = scaled(theme->assets[UI_HEADING + i].width, ts);
        s->text[i] = (struct ui_text_style){NULL, size > 256 ? 256 : size,
            (int64_t)l[UI_HEADING_TRACK + i] * ts / 65536, i == 1 ? 0 : 1,
            i == 2 ? l[UI_MUTED] : l[UI_TEXT]};
    }
#undef S
    bool enabled;
    ui_document_enabled(doc, &enabled);
    ui_document_get(doc, "", "theme");
    const char *v = ui_document_get(doc, "", "title");
    if (v != NULL) {
        s->title = v;
    }
    v = ui_document_get(doc, "", "version");
    if (v != NULL && strcmp(v, "1") != 0) {
        error(doc, "unsupported configuration version");
    }
    rect(doc, &s->menu, "menu", width, height, scale, true);
    int min_width = dimension(doc, "menu", "min_width", 1, width, scale, true);
    int max_width = dimension(doc, "menu", "max_width", width, width, scale, true);
    if (min_width < 1 || max_width < min_width || max_width > width) {
        error(doc, "invalid menu width constraints");
    }
    if (s->menu.width < min_width) { s->menu.width = min_width; }
    if (s->menu.width > max_width) { s->menu.width = max_width; }
    anchor(doc, &s->menu, "menu");
    s->padding = dimension(doc, "menu", "padding", 0, 0, scale, false);
#define D(field, section, key) s->field = dimension(doc, section, key, s->field, 0, scale, false)
    D(row_height, "menu.item", "height"); D(row_gap, "menu.item", "gap");
    D(indent, "menu.item", "indent"); D(icon_x, "menu.item", "icon_x");
    D(icon_size, "menu.item", "icon_size"); D(label_x, "menu.item.label", "x");
    D(label_baseline, "menu.item.label", "baseline");
    D(chevron_size, "menu.item", "indicator_size");
    int inner = s->menu.width - s->padding * 2;
    if (ui_document_get(doc, "menu", "width") != NULL || s->padding != 0) {
        s->chevron_x = inner - s->chevron_size - scaled(24, scale);
        s->label_width = s->chevron_x - s->label_x - scaled(32, scale);
    }
    D(label_width, "menu.item.label", "width"); D(chevron_x, "menu.item", "indicator_x");
#undef D
    text_style(doc, &s->text[0], "heading", ts);
    text_style(doc, &s->text[1], "menu.item.label", ts);
    text_style(doc, &s->text[2], "hints", ts);
    colour(doc, "menu.item.selected", "text_colour", &s->selected_colour);
    surface(doc, &s->rows[0], "menu.item", scale);
    surface(doc, &s->rows[1], "menu.item.selected", scale);
    s->glow_colour = s->marker_colour = s->rows[1].border;
    s->glow_width = dimension(doc, "menu.item.selected", "glow_width", 0, 0, scale, false);
    s->glow_opacity = 64;
    opacity(doc, "menu.item.selected", "glow_opacity", &s->glow_opacity);
    colour(doc, "menu.item.selected", "glow_colour", &s->glow_colour);
    s->marker_size = dimension(doc, "menu.item.selected", "marker_size", scaled(16, scale), 0, scale, false);
    colour(doc, "menu.item.selected", "marker_colour", &s->marker_colour);
    v = ui_document_get(doc, "menu.item.selected", "marker");
    if (v != NULL) {
        if (strcmp(v, "none") == 0) { s->marker_side = 0; }
        else if (strcmp(v, "diamond-left") == 0) { s->marker_side = 1; }
        else if (strcmp(v, "diamond-right") == 0) { s->marker_side = 2; }
        else { error(doc, "expected none, diamond-left or diamond-right marker"); }
    }
    if (s->glow_width < 0 || s->glow_width > 32 || s->marker_size < 0
     || s->marker_size > 128 || (s->marker_side != 0 && s->marker_size > s->row_height)) {
        error(doc, "selection glow or marker out of range");
    }
    v = ui_document_get(doc, "menu.item.label", "overflow");
    if (v != NULL && strcmp(v, "ellipsis") != 0) {
        error(doc, "only ellipsis overflow is supported");
    }
    s->heading_file = ui_document_get(doc, "heading", "file");
    v = ui_document_get(doc, "heading", "mode");
    if (v != NULL) {
        if (strcmp(v, "image") == 0) { s->heading_image = true; }
        else if (strcmp(v, "text") != 0) { error(doc, "expected text or image heading mode"); }
    }
    v = ui_document_get(doc, "heading", "fit");
    if (v != NULL) {
        if (strcmp(v, "stretch") == 0) { s->heading_stretch = true; }
        else if (strcmp(v, "contain") != 0) { error(doc, "expected contain or stretch heading fit"); }
    }
    rect(doc, &s->heading, "heading", width, height, scale, true);
    if (s->heading_image && (s->heading_file == NULL || *s->heading_file == 0 || s->heading.height <= 0)) {
        error(doc, "image heading requires a file and positive height");
    }
    rect(doc, &s->divider, "divider", width, height, scale, true);
    support_settings(s, doc, width, height, scale, ts);
    rect(doc, &s->countdown, "countdown", width, height, scale, false);
    boolean(doc, "heading", "visible", &s->heading_visible);
    boolean(doc, "divider", "visible", &s->divider_visible);
    boolean(doc, "hints", "visible", &s->hints_visible);
    boolean(doc, "details", "visible", &s->details_visible);
    boolean(doc, "background", "visible", &s->wallpaper);
    colour(doc, "background", "colour", &s->background_colour);
    s->background_file = ui_document_get(doc, "background", "file");
    v = ui_document_get(doc, "background", "fit");
    if (v != NULL) {
        if (strcmp(v, "cover") == 0) { s->background_fit = 0; }
        else if (strcmp(v, "contain") == 0) { s->background_fit = 1; }
        else if (strcmp(v, "stretch") == 0) { s->background_fit = 2; }
        else { error(doc, "unknown background fit"); }
    }
    s->cursor_file = ui_document_get(doc, "cursor", "file");
    s->cursor_width = dimension(doc, "cursor", "width", 24, 0, scale, false);
    s->cursor_height = dimension(doc, "cursor", "height", 32, 0, scale, false);
    s->cursor_hotspot_x = dimension(doc, "cursor", "hotspot_x", 0, 0, scale, false);
    s->cursor_hotspot_y = dimension(doc, "cursor", "hotspot_y", 0, 0, scale, false);
    if (s->cursor_width < 1 || s->cursor_width > 128 || s->cursor_height < 1 || s->cursor_height > 128
     || s->cursor_hotspot_x < 0 || s->cursor_hotspot_x >= s->cursor_width
     || s->cursor_hotspot_y < 0 || s->cursor_hotspot_y >= s->cursor_height) {
        error(doc, "cursor dimensions or hotspot out of range");
    }
    for (size_t i = 0; i < doc->count; i++) {
        struct ui_property *p = &doc->fields[i];
        if (p->used) { continue; }
        if (doc->error == NULL) { doc->error_line = p->line; }
        if (strncmp(p->section, "entry \"", 7) == 0 && strcmp(p->key, "icon") == 0
         && s->icon_count < UI_CONFIG_MAX_ICONS && *p->value != 0) {
            p->section[strlen(p->section) - 1] = 0;
            s->icons[s->icon_count++] = (struct ui_icon_rule){p->section + 7, p->value};
            p->used = true;
        } else {
            return error(doc, "unknown property or too many icons");
        }
    }
    if (doc->error != NULL) { return false; }
    doc->error_line = 0;
    if (s->padding < 0 || s->padding > 256 || s->row_height < 16 || s->row_height > 512
     || s->row_gap < 0 || s->row_gap > 256 || s->menu.x < 0 || s->menu.y < 0
     || s->menu.width < 64 || s->menu.height < s->row_height + 2 * s->padding
     || s->menu.x + s->menu.width > width || s->menu.y + s->menu.height > height
     || inner < 64 || s->label_x < 0 || s->label_width <= 0
     || s->label_x + s->label_width > inner || s->label_baseline < 0 || s->label_baseline > s->row_height
     || s->icon_x < 0 || s->icon_size < 0 || s->icon_size > s->row_height
     || s->icon_x + s->icon_size > inner || s->chevron_x < 0 || s->chevron_size < 0
     || s->chevron_size > s->row_height || s->chevron_x + s->chevron_size > inner
     || s->indent < 0 || s->indent > 256) {
        return error(doc, "menu or item geometry does not fit");
    }
    s->max_rows = (s->menu.height - 2 * s->padding + s->row_gap) / (s->row_height + s->row_gap);
    const struct ui_rect *boxes[] = {&s->heading, &s->hints, &s->details, &s->countdown, &s->divider};
    for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); i++) {
        const struct ui_rect *b = boxes[i];
        if (b == &s->details && s->detail_selection) { continue; }
        if (b->x < 0 || b->y < 0 || b->width < 1 || b->height < 0
         || b->x + b->width > width || b->y + b->height > height) {
            return error(doc, "text or divider geometry does not fit");
        }
    }
    return true;
}

bool ui_config_path(char *out, size_t capacity, const char *config, const char *asset) {
    if (config == NULL || asset == NULL || *asset == 0) {
        return false;
    }
    size_t prefix = 0;
    if (*asset != '/') {
        const char *slash = strrchr(config, '/');
        if (slash != NULL) {
            prefix = slash - config + 1;
        }
    }
    size_t size = strlen(asset);
    if (prefix + size >= capacity) {
        return false;
    }
    memcpy(out, config, prefix);
    memcpy(out + prefix, asset, size + 1);
    return true;
}
