#include <ui/support.h>
#include <lib/libc.h>

static struct ui_canvas clipped(struct ui_canvas *canvas, struct ui_rect box) {
    struct ui_canvas c = *canvas;
    int right = box.x + box.width, bottom = box.y + box.height;
    if (!c.clipped || c.clip_left < box.x) { c.clip_left = box.x; }
    if (!c.clipped || c.clip_top < box.y) { c.clip_top = box.y; }
    if (!c.clipped || c.clip_right > right) { c.clip_right = right; }
    if (!c.clipped || c.clip_bottom > bottom) { c.clip_bottom = bottom; }
    c.clipped = true;
    return c;
}

static void panel(struct ui_canvas *c, struct ui_rect box, const struct ui_surface *s) {
    if (s->opacity == 0 && s->border_width == 0) { return; }
    ui_surface(c, box.x, box.y, box.width, box.height,
        (uint32_t)s->opacity << 24 | s->fill, 0xff000000 | s->border, s->border_width, s->radius);
}

static void line(struct ui_canvas *c, struct ui_font *font, const struct ui_text_style *t,
    const char *text, int x, int baseline, int width, uint32_t colour, int align) {
    if (width <= 0) { return; }
    int advance = ui_text_width(font, text, t->tracking);
    int offset = advance < width ? (width - advance) * align / 2 : 0;
    ui_text(c, font, text, x + offset, baseline, t->tracking, colour, width - offset);
}

static int key_width(struct ui_font *font, const struct ui_settings *s, const char *key) {
    return s->key_width != 0 ? s->key_width : ui_text_width(font, key, s->text[2].tracking) + 2 * s->key_padding;
}

static int hint_width(struct ui_font *font, const struct ui_settings *s, const char *key, const char *label) {
    int kw = key_width(font, s, key), lw = ui_text_width(font, label, s->text[2].tracking);
    if (*label == 0) { return kw; }
    return s->key_position == 2 ? (kw > lw ? kw : lw) : kw + s->key_gap + lw;
}

static void hint(struct ui_canvas *canvas, struct ui_font *font, const struct ui_settings *s,
    struct ui_rect box, const char *key, const char *label, int align) {
    struct ui_canvas c = clipped(canvas, box);
    const struct ui_text_style *t = &s->text[2];
    int kw = key_width(font, s, key), total = hint_width(font, s, key, label);
    if (total < box.width) { box.x += (box.width - total) * align / 2; box.width = total; }
    if (kw > box.width) { kw = box.width; }
    struct ui_rect cap = {box.x, box.y, kw, box.height};
    int baseline = box.y + (box.height - t->size) / 2 + t->size * 3 / 4;
    if (s->key_position == 2 && *label != 0) {
        cap.height = (box.height - s->key_gap) / 2;
        cap.x += (box.width - kw) * align / 2;
        baseline = cap.y + (cap.height - t->size) / 2 + t->size * 3 / 4;
        line(&c, font, t, label, box.x, box.y + cap.height + s->key_gap
            + (cap.height - t->size) / 2 + t->size * 3 / 4, box.width, t->colour, align);
    } else if (s->key_position == 1 && *label != 0) {
        cap.x += box.width - kw;
        line(&c, font, t, label, box.x, baseline, box.width - kw - s->key_gap, t->colour, 0);
    } else if (*label != 0) {
        line(&c, font, t, label, box.x + kw + s->key_gap, baseline,
            box.width - kw - s->key_gap, t->colour, 0);
    }
    panel(&c, cap, &s->key_surface);
    line(&c, font, t, key, cap.x + s->key_padding, baseline,
        cap.width - 2 * s->key_padding, s->key_colour, 1);
}

void ui_draw_hints(struct ui_canvas *canvas, struct ui_font *font,
    const struct ui_settings *s, const char *const labels[UI_HINT_COUNT]) {
    static const char *keys[] = {"Arrows", "Enter", "E", "B", "S", "U"};
    struct ui_rect box = s->hints;
    struct ui_canvas c = clipped(canvas, box);
    panel(&c, box, &s->hint_panel);
    box.x += s->hint_padding; box.y += s->hint_padding;
    box.width -= 2 * s->hint_padding; box.height -= 2 * s->hint_padding;
    c = clipped(&c, box);
    int actions[UI_HINT_COUNT], widths[UI_HINT_COUNT], count = 0, total = 0;
    const char *text[UI_HINT_COUNT];
    for (int i = 0; i < UI_HINT_COUNT; i++) {
        int action = s->hint_order[i];
        if (!s->hint_items[action].visible || labels[action] == NULL) { continue; }
        actions[count] = action;
        text[count] = s->hint_items[action].label != NULL ? s->hint_items[action].label : labels[action];
        widths[count] = hint_width(font, s, keys[action], text[count]);
        total += widths[count++];
    }
    if (count == 0) { return; }
    total += (count - 1) * s->hint_item_gap;
    int x = box.x;
    if (s->hint_layout == UI_HINT_HORIZONTAL && total < box.width) { x += (box.width - total) * s->text[2].align / 2; }
    for (int i = 0; i < count; i++) {
        struct ui_rect item = {box.x, box.y, box.width, s->hint_item_height};
        int align = s->text[2].align;
        if (s->hint_layout == UI_HINT_CUSTOM) {
            item = s->hint_items[actions[i]].box;
            item.x += box.x; item.y += box.y;
        } else if (s->hint_layout == UI_HINT_HORIZONTAL) {
            item.x = x;
            item.width = total <= box.width ? widths[i] : (box.width - (count - 1) * s->hint_item_gap) / count;
            x += item.width + s->hint_item_gap;
            align = 0;
        } else if (s->hint_layout == UI_HINT_GRID) {
            item.width = (box.width - (s->hint_columns - 1) * s->hint_item_gap) / s->hint_columns;
            item.x += (i % s->hint_columns) * (item.width + s->hint_item_gap);
            item.y += (i / s->hint_columns) * (item.height + s->hint_item_gap);
        } else {
            item.y += i * (item.height + s->hint_item_gap);
        }
        if (item.width > 0) { hint(&c, font, s, item, keys[actions[i]], text[i], align); }
    }
}

void ui_draw_details(struct ui_canvas *canvas, struct ui_font *font,
    const struct ui_settings *s, const char *text, const struct ui_rect *selection) {
    if (text == NULL || *text == 0) { return; }
    const struct ui_text_style *t = &s->text[3];
    struct ui_rect box = s->details;
    if (s->detail_selection) {
        if (selection == NULL) { return; }
        box.x += selection->x; box.y += selection->y;
    }
    if (box.height == 0) {
        line(canvas, font, t, text, box.x, box.y, box.width, t->colour, t->align);
        return;
    }
    struct ui_canvas c = clipped(canvas, box);
    panel(&c, box, &s->detail_panel);
    box.x += s->detail_padding; box.y += s->detail_padding;
    box.width -= 2 * s->detail_padding; box.height -= 2 * s->detail_padding;
    c = clipped(&c, box);
    int lines = s->detail_wrap ? (box.height + s->detail_line_gap) / (t->size + s->detail_line_gap) : 1;
    if (lines > 64) { lines = 64; }
    if (!s->detail_wrap) {
        line(&c, font, t, text, box.x, box.y + t->size * 3 / 4, box.width, t->colour, t->align);
        return;
    }
    size_t consumed = 0;
    for (int row = 0; row < lines && *text != 0 && consumed < 4096; row++) {
        char buffer[1028];
        size_t length = 0, space = 0;
        const char *start = text;
        while (*text != 0 && *text != '\n' && length < 1020 && consumed + length < 4096) {
            size_t bytes = 1;
            while (bytes < 4 && text[bytes] != 0 && ((unsigned char)text[bytes] & 0xc0) == 0x80) { bytes++; }
            if (bytes > 4096 - consumed - length) { consumed = 4096; break; }
            memcpy(buffer + length, text, bytes);
            buffer[length + bytes] = 0;
            if (ui_text_width(font, buffer, t->tracking) > box.width && length != 0) {
                if (space != 0 && *text != ' ') { length = space; text = start + space; }
                break;
            }
            if (*text == ' ') { space = length; }
            length += bytes;
            text += bytes;
        }
        consumed += text - start;
        while (*text == ' ' && consumed < 4096) { text++; consumed++; }
        if (*text == '\n' && consumed < 4096) { text++; consumed++; }
        while (length > 0 && buffer[length - 1] == ' ') { length--; }
        buffer[length] = 0;
        if (*text != 0 && (row == lines - 1 || consumed >= 4096)) { strcpy(buffer + length, "\xe2\x80\xa6"); }
        line(&c, font, t, buffer, box.x, box.y + t->size * 3 / 4 + row * (t->size + s->detail_line_gap), box.width, t->colour, t->align);
    }
}
