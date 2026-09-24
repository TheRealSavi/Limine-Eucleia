#include <menu_renderer.h>
#include <ui/canvas.h>
#include <ui/display.h>
#include <ui/config.h>
#include <ui/assets.h>
#include <ui/support.h>
#include <lib/config.h>
#include <lib/fb.h>
#include <lib/gterm.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <mm/pmm.h>
#include <drivers/mouse.h>

static struct ui_theme theme;
static struct ui_font *fonts[4];
static void *font_data[4];
static size_t font_sizes[4];
static struct ui_canvas frame;
static uint32_t *background, *timeout_backing, *pointer_backing;
static size_t timeout_size, pointer_size;
static struct ui_rect timeout_box, pointer_box;
static void *theme_data;
static size_t theme_size, canvas_size, config_size;
static char *config_data;
static struct ui_document *document;
static struct ui_settings *settings;
static struct ui_asset *images;
enum { BACKGROUND_IMAGE = UI_CONFIG_MAX_ICONS, HEADING_IMAGE, CURSOR_IMAGE, IMAGE_COUNT };
static struct ui_display display;
static struct menu_style style;
static size_t first_row, row_count, visible_rows;
static bool active, dirty, pointer_drawn, terminal_fallback;
static char config_path[512], notice[192];

static void gui_leave(void);

const char *menu_gui_notice(void) {
    return notice;
}

static void set_notice(const char *message, unsigned line) {
    strcpy(notice, "Eucleia: ");
    size_t at = strlen(notice);
    if (line != 0) {
        strcpy(notice + at, "line ");
        at += 5;
        char digits[16];
        size_t count = 0;
        do {
            digits[count++] = '0' + line % 10;
            line /= 10;
        } while (line != 0);
        while (count != 0) { notice[at++] = digits[--count]; }
        notice[at++] = ':';
        notice[at++] = ' ';
    }
    while (*message != 0 && at < sizeof(notice) - 1) { notice[at++] = *message++; }
    notice[at] = 0;
}

static void text_at(const char *value, unsigned role, struct ui_rect box, uint32_t colour) {
    struct ui_text_style *t = &settings->text[role];
    int width = ui_text_width(fonts[role], value, t->tracking);
    int x = box.x;
    if (width < box.width) { x += (box.width - width) * t->align / 2; }
    ui_text(&frame, fonts[role], value, x, box.y, t->tracking, colour, box.width - (x - box.x));
}

static void text_left(const char *value, unsigned role, struct ui_rect box, uint32_t colour) {
    ui_text(&frame, fonts[role], value, box.x, box.y, settings->text[role].tracking, colour, box.width);
}

static void release(void) {
    if (fonts[3] == fonts[2]) { fonts[3] = NULL; }
    for (size_t i = 0; i < 4; i++) {
        ui_font_close(fonts[i]);
        fonts[i] = NULL;
        if (font_data[i] != NULL) {
            pmm_free(font_data[i], font_sizes[i] + 1);
            font_data[i] = NULL;
        }
    }
    if (images != NULL) {
        for (size_t i = 0; i < IMAGE_COUNT; i++) { ui_png_free(&images[i]); }
        pmm_free(images, IMAGE_COUNT * sizeof(*images));
        images = NULL;
    }
    if (theme_data != NULL) { pmm_free(theme_data, theme_size + 1); theme_data = NULL; }
    if (config_data != NULL) { pmm_free(config_data, config_size + 1); config_data = NULL; }
    if (document != NULL) { pmm_free(document, sizeof(*document)); document = NULL; }
    if (settings != NULL) { pmm_free(settings, sizeof(*settings)); settings = NULL; }
    if (frame.pixels != NULL) { pmm_free(frame.pixels, canvas_size); frame.pixels = NULL; }
    if (background != NULL) { pmm_free(background, canvas_size); background = NULL; }
    if (timeout_backing != NULL) { pmm_free(timeout_backing, timeout_size); timeout_backing = NULL; }
    if (pointer_backing != NULL) { pmm_free(pointer_backing, pointer_size); pointer_backing = NULL; }
}

static bool fail(const char *message, unsigned line) {
    set_notice(message, line);
    release();
    return false;
}

static bool gui_init(const struct menu_style *options) {
    release();
    notice[0] = 0;
    active = pointer_drawn = terminal_fallback = false;
    frame.clipped = false;
    if (quiet || SERIAL_CONSOLE || term_backend != GTERM || fb_fbs_count == 0
     || !ui_config_path(config_path, sizeof(config_path), config_get_path(), "eucleia.conf")) {
        return false;
    }
    bool old_case = case_insensitive_fopen;
    case_insensitive_fopen = true;
    struct file_handle *file = fopen(boot_volume, config_path);
    case_insensitive_fopen = old_case;
    if (file == NULL) { return false; }
    if (file->size == 0 || file->size > UI_CONFIG_MAX_SIZE) {
        fclose(file);
        return fail("configuration must be 1..32768 bytes", 0);
    }
    config_size = file->size;
    config_data = ext_mem_alloc(config_size + 1);
    bool read_ok = fread(file, config_data, 0, config_size) == config_size;
    fclose(file);
    config_data[config_size] = 0;
    if (!read_ok) { return fail("cannot read configuration", 0); }
    document = ext_mem_alloc(sizeof(*document));
    bool enabled;
    if (!ui_document_read(document, config_data, config_size) || !ui_document_enabled(document, &enabled)) {
        return fail(document->error, document->error_line);
    }
    if (!enabled) { release(); return false; }
    if (!ui_display_init(&display, &fb_fbs[0], gterm_get_rotation(NULL))) {
        release();
        return false;
    }
    const char *theme_file = ui_document_get(document, "", "theme");
    char path[512];
    if (!ui_config_path(path, sizeof(path), config_path, theme_file != NULL ? theme_file : "/boot/eucleia.eui")) {
        return fail("invalid theme path", 0);
    }
    theme_data = ui_read_file(path, UI_THEME_MAX_SIZE, &theme_size);
    if (theme_data == NULL || !ui_theme_read(&theme, theme_data, theme_size)) {
        return fail("missing or invalid theme pack", 0);
    }
    settings = ext_mem_alloc(sizeof(*settings));
    if (!ui_settings_read(settings, document, &theme, display.width, display.height)) {
        return fail(document->error, document->error_line);
    }
    style = *options;
    frame.width = display.width;
    frame.height = display.height;
    canvas_size = (size_t)frame.width * frame.height * 4;
    frame.pixels = ext_mem_alloc(canvas_size);
    background = ext_mem_alloc(canvas_size);
    for (size_t i = 0; i < 4; i++) {
        const struct ui_text_style *t = settings->text;
        if (i == 3 && t[3].size == t[2].size
         && (t[3].file == t[2].file || (t[3].file != NULL && t[2].file != NULL && strcmp(t[3].file, t[2].file) == 0))) {
            fonts[3] = fonts[2];
            continue;
        }
        const struct ui_asset *asset = &theme.assets[UI_HEADING + (i == 3 ? 2 : i)];
        const void *data = asset->pixels;
        size_t size = asset->size;
        if (settings->text[i].file != NULL) {
            if (!ui_config_path(path, sizeof(path), config_path, settings->text[i].file)) {
                return fail("invalid font path", 0);
            }
            font_data[i] = ui_read_file(path, 2 * 1024 * 1024, &font_sizes[i]);
            if (font_data[i] == NULL) { return fail("cannot read font file", 0); }
            data = font_data[i]; size = font_sizes[i];
        }
        fonts[i] = ui_font_open(data, size, settings->text[i].size);
        if (fonts[i] == NULL) { return fail("invalid font or exhausted font budget", 0); }
    }
    images = ext_mem_alloc(IMAGE_COUNT * sizeof(*images));
    size_t remaining = 16 * 1024 * 1024;
    const struct ui_asset *wallpaper = &theme.assets[UI_BACKGROUND];
    if (settings->background_file != NULL) {
        if (!ui_config_path(path, sizeof(path), config_path, settings->background_file)
         || !ui_png_read(&images[BACKGROUND_IMAGE], path, 4096, &remaining)) {
            return fail("invalid background PNG or image budget exceeded", 0);
        }
        wallpaper = &images[BACKGROUND_IMAGE];
    }
    if (settings->heading_visible && settings->heading_image
     && (!ui_config_path(path, sizeof(path), config_path, settings->heading_file)
      || !ui_png_read(&images[HEADING_IMAGE], path, 2048, &remaining))) {
        return fail("invalid heading PNG or image budget exceeded", 0);
    }
    if (settings->cursor_file != NULL
     && (!ui_config_path(path, sizeof(path), config_path, settings->cursor_file)
      || !ui_png_read(&images[CURSOR_IMAGE], path, 256, &remaining))) {
        set_notice("cursor PNG unavailable; using the built-in pointer", 0);
    }
    if (images[CURSOR_IMAGE].pixels == NULL) {
        settings->cursor_width = 16;
        settings->cursor_height = 23;
        settings->cursor_hotspot_x = settings->cursor_hotspot_y = 0;
    }
    pointer_size = (size_t)settings->cursor_width * settings->cursor_height * 4;
    pointer_backing = ext_mem_alloc(pointer_size);
    for (size_t i = 0; i < settings->icon_count; i++) {
        // The resource is owned by its first rule; later rules share it at draw time.
        bool shared = false;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(settings->icons[j].file, settings->icons[i].file) == 0) { shared = true; break; }
        }
        if (!shared && (!ui_config_path(path, sizeof(path), config_path, settings->icons[i].file)
         || !ui_png_read(&images[i], path, 256, &remaining))) {
            set_notice("an entry icon is unavailable; using its default", 0);
        }
    }
    ui_rect(&frame, 0, 0, frame.width, frame.height, settings->background_colour | 0xff000000);
    if (settings->wallpaper) {
        int width = frame.width, height = frame.height;
        if (settings->background_fit != 2) {
            height = (uint64_t)width * wallpaper->height / wallpaper->width;
            if ((settings->background_fit == 0 && height < frame.height)
             || (settings->background_fit == 1 && height > frame.height)) {
                height = frame.height;
                width = (uint64_t)height * wallpaper->width / wallpaper->height;
            }
        }
        ui_image(&frame, wallpaper, (frame.width - width) / 2, (frame.height - height) / 2, width, height);
    }
    if (settings->heading_visible) {
        if (settings->heading_image) {
            const struct ui_asset *asset = &images[HEADING_IMAGE];
            struct ui_rect r = settings->heading;
            int width = r.width, height = r.height;
            if (!settings->heading_stretch) {
                height = (uint64_t)width * asset->height / asset->width;
                if (height > r.height) {
                    height = r.height;
                    width = (uint64_t)height * asset->width / asset->height;
                }
            }
            ui_image(&frame, asset, r.x + (r.width - width) / 2,
                r.y + (r.height - height) / 2, width, height);
        } else {
            text_at(settings->title, 0, settings->heading, settings->text[0].colour);
        }
    }
    if (settings->divider_visible) {
        struct ui_rect r = settings->divider;
        ui_image(&frame, &theme.assets[UI_DIVIDER], r.x, r.y, r.width, r.height);
    }
    memcpy(background, frame.pixels, canvas_size);
    first_row = row_count = visible_rows = 0;
    timeout_box = settings->countdown;
    timeout_box.y -= settings->text[2].size;
    if (timeout_box.y < 0) { timeout_box.y = 0; }
    timeout_box.height = settings->text[2].size * 2;
    if (timeout_box.y + timeout_box.height > frame.height) { timeout_box.height = frame.height - timeout_box.y; }
    timeout_size = (size_t)timeout_box.width * timeout_box.height * 4;
    if (timeout_size != 0) { timeout_backing = ext_mem_alloc(timeout_size); }
    return true;
}

static void countdown_copy(bool save) {
    for (int y = 0; y < timeout_box.height; y++) {
        uint32_t *pixels = frame.pixels + (size_t)(timeout_box.y + y) * frame.width + timeout_box.x;
        uint32_t *backing = timeout_backing + (size_t)y * timeout_box.width;
        memcpy(save ? backing : pixels, save ? pixels : backing, timeout_box.width * 4);
    }
}

static void gui_draw(const struct menu_view *view) {
    if (terminal_fallback) { menu_terminal_renderer.draw(view); return; }
    if (!active) {
        mouse_set_canvas(display.fb->framebuffer_width, display.fb->framebuffer_height);
        for (size_t i = 1; i < fb_fbs_count; i++) { fb_clear(&fb_fbs[i]); }
        active = true;
    }
    FOR_TERM(TERM->cursor_enabled = false);
    FOR_TERM(TERM->autoflush = false);
    memcpy(frame.pixels, background, canvas_size);
    row_count = view->count;
    visible_rows = settings->max_rows;
    if (visible_rows > row_count) { visible_rows = row_count; }
    if (view->selected < first_row) { first_row = view->selected; }
    if (visible_rows != 0 && view->selected >= first_row + visible_rows) {
        first_row = view->selected - visible_rows + 1;
    }
    int x = settings->menu.x + settings->padding;
    int top = settings->menu.y + settings->padding;
    int width = settings->menu.width - 2 * settings->padding;
    int height = settings->row_height, pitch = height + settings->row_gap;
    for (size_t i = 0; i < visible_rows; i++) {
        size_t index = first_row + i;
        const struct menu_row *row = &view->rows[index];
        int y = top + i * pitch;
        bool selected = index == view->selected;
        struct ui_surface *surface = &settings->rows[selected];
        uint32_t glow = settings->glow_colour | (uint32_t)settings->glow_opacity << 24;
        if (selected) {
            ui_glow(&frame, x, y, width, height, surface->radius, settings->glow_width, glow);
        }
        ui_surface(&frame, x, y, width, height, surface->fill | (uint32_t)surface->opacity << 24,
            surface->border | 0xff000000, surface->border_width, surface->radius);
        if (selected && settings->marker_side != 0) {
            int mx = settings->marker_side == 1 ? x : x + width;
            ui_diamond(&frame, mx, y + height / 2, settings->marker_size,
                settings->marker_colour, settings->glow_width, glow);
        }
        frame.clipped = true;
        frame.clip_left = x; frame.clip_top = y; frame.clip_right = x + width; frame.clip_bottom = y + height;
        const struct ui_asset *icon = &theme.assets[row->directory ? UI_FOLDER_ICON : UI_BOOT_ICON];
        for (size_t rule = 0; rule < settings->icon_count; rule++) {
            if (menu_row_matches(view->rows, index, settings->icons[rule].entry)) {
                size_t owner = rule;
                for (size_t j = 0; j < rule; j++) {
                    if (strcmp(settings->icons[j].file, settings->icons[rule].file) == 0) { owner = j; break; }
                }
                if (images[owner].pixels != NULL) { icon = &images[owner]; }
                break;
            }
        }
        ui_image(&frame, icon, x + settings->icon_x, y + (height - settings->icon_size) / 2,
            settings->icon_size, settings->icon_size);
        size_t depth = row->depth > 4 ? 4 : row->depth;
        int indent = depth * settings->indent;
        struct ui_rect label = {x + settings->label_x + indent, y + settings->label_baseline,
            settings->label_width - indent, 0};
        text_at(row->name, 1, label, selected ? settings->selected_colour : settings->text[1].colour);
        if (row->directory || selected) {
            int size = settings->chevron_size, cx = x + settings->chevron_x;
            if (row->directory && row->expanded) {
                text_at("-", 1, (struct ui_rect){cx, y + settings->label_baseline, size, 0}, settings->rows[1].border);
            } else {
                ui_image(&frame, &theme.assets[UI_CHEVRON], cx, y + (height - size) / 2, size, size);
            }
        }
        frame.clipped = false;
    }
    if (view->count == 0) {
        text_at(view->config_ready ? "No boot entries available" : "Configuration not found", 1,
            (struct ui_rect){x, top + height, width, 0}, settings->text[1].colour);
    }
    if (first_row != 0) {
        text_left("More above", 2, (struct ui_rect){x, top - 9, width, 0}, settings->text[2].colour);
    }
    if (first_row + visible_rows < view->count) {
        text_left("More below", 2, (struct ui_rect){x, top + visible_rows * pitch + 7, width, 0}, settings->text[2].colour);
    }
    if (view->comment != NULL && settings->details_visible) {
        struct ui_rect selected = {x, top + (view->count != 0 ? (int)(view->selected - first_row) * pitch : 0), width, height};
        ui_draw_details(&frame, fonts[3], settings, view->comment, view->count != 0 ? &selected : NULL);
    }
    if (!style.help_hidden && settings->hints_visible && settings->hint_layout != UI_HINT_LEGACY) {
        bool entry = view->count != 0;
        bool directory = entry && view->rows[view->selected].directory;
        const char *labels[UI_HINT_COUNT] = {
            entry ? "Select" : NULL,
            directory ? (view->rows[view->selected].expanded ? "Collapse" : "Expand") : (entry ? "Boot" : NULL),
            style.editor_enabled && entry && !directory ? "Edit" : NULL,
            style.editor_enabled ? "Blank entry" : NULL,
            style.firmware_setup ? "Firmware" : NULL,
            style.uefi_shell ? "Shell" : NULL
        };
        ui_draw_hints(&frame, fonts[2], settings, labels);
    }
    if (!style.help_hidden && settings->hints_visible && settings->hint_layout == UI_HINT_LEGACY) {
        struct ui_rect hint = settings->hints;
        ui_rect(&frame, hint.x, hint.y - settings->text[2].size - 3, hint.width, 1, 0x806e5038);
        const char *primary = style.editor_enabled ? "Arrows  Select     Enter  Boot     E  Edit" : "Arrows  Select     Enter  Boot";
        if (view->count != 0 && view->rows[view->selected].directory) {
            primary = view->rows[view->selected].expanded ? "Arrows  Select     Enter  Collapse" : "Arrows  Select     Enter  Expand";
        } else if (view->count == 0) { primary = "Check your boot configuration"; }
        text_at(primary, 2, hint, settings->text[2].colour);
        char secondary[80] = "";
        if (style.editor_enabled) { strcpy(secondary, "B  Blank entry"); }
        if (style.firmware_setup) { strcpy(secondary + strlen(secondary), "     S  Firmware"); }
        if (style.uefi_shell) { strcpy(secondary + strlen(secondary), "     U  Shell"); }
        hint.y += settings->hint_gap;
        text_at(secondary, 2, hint, settings->text[2].colour);
    }
    if (notice[0] != 0) {
        ui_rect(&frame, 0, frame.height - settings->text[2].size * 2, frame.width,
            settings->text[2].size * 2, 0xff101010);
        text_at(notice, 2, (struct ui_rect){8, frame.height - 8, frame.width - 16, 0}, 0xffffff);
    }
    for (size_t i = 0; i < 4; i++) {
        if (ui_font_failed(fonts[i])) {
            gui_leave();
            release();
            set_notice("font rendering failed; using terminal", 0);
            terminal_fallback = true;
            menu_terminal_renderer.init(&style);
            menu_terminal_renderer.draw(view);
            return;
        }
    }
    countdown_copy(true);
    dirty = true;
}
static void blit(int x, int y, int width, int height) {
    ui_display_blit(&display, &frame, x, y, width, height);
}

static void gui_present(void) {
    if (terminal_fallback) {
        menu_terminal_renderer.present();
        return;
    }
    if (!active) {
        return;
    }
    if (dirty) {
        blit(0, 0, frame.width, frame.height);
        dirty = false;
    } else if (pointer_drawn) {
        blit(pointer_box.x, pointer_box.y, pointer_box.width, pointer_box.height);
    }
    size_t x, y;
    bool visible;
    mouse_get_position(&x, &y, &visible);
    int logical_x, logical_y;
    visible = visible && ui_display_position(&display, x, y, &logical_x, &logical_y);
    pointer_drawn = visible;
    if (!visible) {
        return;
    }
    int left = logical_x - settings->cursor_hotspot_x;
    int top = logical_y - settings->cursor_hotspot_y;
    int right = left + settings->cursor_width, bottom = top + settings->cursor_height;
    if (right > frame.width) { right = frame.width; }
    if (bottom > frame.height) { bottom = frame.height; }
    pointer_box.x = left < 0 ? 0 : left;
    pointer_box.y = top < 0 ? 0 : top;
    pointer_box.width = right - pointer_box.x;
    pointer_box.height = bottom - pointer_box.y;
    for (int dy = 0; dy < pointer_box.height; dy++) {
        memcpy(pointer_backing + dy * pointer_box.width,
            frame.pixels + (size_t)(pointer_box.y + dy) * frame.width + pointer_box.x,
            pointer_box.width * 4);
    }
    if (images[CURSOR_IMAGE].pixels != NULL) {
        ui_image(&frame, &images[CURSOR_IMAGE], left, top, settings->cursor_width, settings->cursor_height);
    } else {
        for (int dy = 0; dy < 23 && top + dy < frame.height; dy++) {
            int end = dy < 17 ? dy * 2 / 3 + 1 : 5;
            for (int dx = 0; dx < end && left + dx < frame.width; dx++) {
                uint32_t colour = dx == 0 || dx == end - 1 || dy == 22 ? 0x100f0d : 0xfff0dc;
                frame.pixels[(size_t)(top + dy) * frame.width + left + dx] = colour;
            }
        }
    }
    blit(pointer_box.x, pointer_box.y, pointer_box.width, pointer_box.height);
    for (int dy = 0; dy < pointer_box.height; dy++) {
        memcpy(frame.pixels + (size_t)(pointer_box.y + dy) * frame.width + pointer_box.x,
            pointer_backing + dy * pointer_box.width, pointer_box.width * 4);
    }
}

static void gui_timeout(uint64_t milliseconds) {
    if (terminal_fallback) { menu_terminal_renderer.timeout(milliseconds); return; }
    countdown_copy(false);
    if (milliseconds != 0) {
        char buffer[64] = "Booting in ", digits[20];
        size_t count = 0, at = strlen(buffer);
        uint64_t seconds = (milliseconds + 999) / 1000;
        do { digits[count++] = '0' + seconds % 10; seconds /= 10; } while (seconds != 0);
        while (count != 0) { buffer[at++] = digits[--count]; }
        strcpy(buffer + at, "s - press a key to cancel");
        frame.clipped = true;
        frame.clip_left = timeout_box.x; frame.clip_top = timeout_box.y;
        frame.clip_right = timeout_box.x + timeout_box.width; frame.clip_bottom = timeout_box.y + timeout_box.height;
        text_left(buffer, 2, settings->countdown, settings->rows[1].border);
        frame.clipped = false;
    }
    dirty = true;
    gui_present();
}

static bool gui_hit_test(size_t x, size_t y, size_t *index) {
    if (terminal_fallback) { return menu_terminal_renderer.hit_test(x, y, index); }
    int lx, ly;
    if (!active || !ui_display_position(&display, x, y, &lx, &ly)) { return false; }
    int left = settings->menu.x + settings->padding, top = settings->menu.y + settings->padding;
    int width = settings->menu.width - 2 * settings->padding;
    int height = settings->row_height, pitch = height + settings->row_gap;
    if (lx < left || lx >= left + width || ly < top) { return false; }
    size_t row = (ly - top) / pitch;
    if (row >= visible_rows || (ly - top) % pitch >= height || first_row + row >= row_count) { return false; }
    *index = first_row + row;
    return true;
}

static void gui_leave(void) {
    if (terminal_fallback) {
        menu_terminal_renderer.leave();
        return;
    }
    mouse_set_canvas(0, 0);
    active = false;
    pointer_drawn = false;
    reset_term();
    FOR_TERM(TERM->autoflush = true);
}

const struct menu_renderer menu_gui_renderer = {
    .init = gui_init, .draw = gui_draw, .present = gui_present,
    .timeout = gui_timeout, .hit_test = gui_hit_test, .leave = gui_leave
};
