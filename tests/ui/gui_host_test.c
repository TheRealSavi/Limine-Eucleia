#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <menu_renderer.h>
#include <ui/canvas.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <fs/file.h>

static bool fallback_init(const struct menu_style *style) {
    (void)style;
    return true;
}
static void fallback_draw(const struct menu_view *view) {
    (void)view;
    assert(!"Unexpected font failure in valid fixture");
}
const struct menu_renderer menu_terminal_renderer = {.init = fallback_init, .draw = fallback_draw};

static uint8_t *pack;
static size_t pack_size, allocations;
static uint8_t *header_png;
static size_t header_size;
static const char *ui_config = "version: 1\ntheme: /theme\n";
static bool missing;
static unsigned rotation;
static size_t pointer_width, pointer_height, pointer_x, pointer_y;
static bool pointer_visible;
bool quiet, serial;
bool case_insensitive_fopen;
int term_backend = GTERM;
struct volume *boot_volume;
struct fb_info *fb_fbs;
size_t fb_fbs_count = 1;
static struct flanterm_context terminal;
static struct flanterm_context *contexts[] = {&terminal};
struct flanterm_context **terms = contexts;
size_t terms_i = 1;

void *ext_mem_alloc(uint64_t size) {
    void *p = calloc(1, size);
    assert(p != NULL);
    allocations++;
    return p;
}

void *ext_mem_alloc_counted(size_t count, size_t size) {
    return ext_mem_alloc(count * size);
}

const char *config_get_path(void) {
    return "/limine.conf";
}

void pmm_free(void *p, uint64_t size) {
    assert(p != NULL && size != 0 && allocations != 0);
    allocations--;
    free(p);
}

char *config_get_value(const char *config, size_t index, const char *key) {
    (void)config; (void)index; (void)key;
    return "/theme";
}

struct file_handle *fopen(struct volume *part, const char *name) {
    (void)part;
    if (missing) {
        return NULL;
    }
    struct file_handle *f = calloc(1, sizeof(*f));
    assert(f != NULL);
    f->fd = strcmp(name, "/eucleia.conf") == 0 ? (void *)ui_config : pack;
    f->size = strcmp(name, "/eucleia.conf") == 0 ? strlen(ui_config) : pack_size;
    if (strcmp(name, "/header.png") == 0) {
        f->fd = header_png;
        f->size = header_size;
    }
    return f;
}

uint64_t fread(struct file_handle *f, void *buf, uint64_t offset, uint64_t length) {
    assert(offset + length <= f->size);
    memcpy(buf, (uint8_t *)f->fd + offset, length);
    return length;
}

void fclose(struct file_handle *f) {
    free(f);
}

int gterm_get_rotation(char *config) {
    (void)config;
    return rotation;
}

bool fb_flush(volatile void *base, size_t length) {
    uintptr_t begin = fb_fbs->framebuffer_addr;
    assert((uintptr_t)base >= begin);
    assert((uintptr_t)base + length <= begin + fb_fbs->framebuffer_pitch * fb_fbs->framebuffer_height);
    return true;
}

void fb_clear(struct fb_info *fb) {
    memset((void *)(uintptr_t)fb->framebuffer_addr, 0,
        fb->framebuffer_pitch * fb->framebuffer_height);
}

void print(const char *format, ...) {
    (void)format;
}

void flanterm_context_reinit(struct flanterm_context *ctx) {
    ctx->cursor_enabled = true;
}

static void flush(struct flanterm_context *ctx) {
    assert(ctx == &terminal);
}

void mouse_set_canvas(size_t width, size_t height) {
    pointer_width = width;
    pointer_height = height;
}

void mouse_get_position(size_t *x, size_t *y, bool *visible) {
    *x = pointer_x; *y = pointer_y; *visible = pointer_visible;
}

static void reject_mutation(size_t offset, uint32_t value) {
    uint32_t saved;
    memcpy(&saved, pack + offset, 4);
    memcpy(pack + offset, &value, 4);
    struct ui_theme theme;
    assert(!ui_theme_read(&theme, pack, pack_size));
    memcpy(pack + offset, &saved, 4);
}

static bool hit(const struct menu_renderer *renderer, size_t x, size_t y, size_t *index) {
    size_t w = fb_fbs->framebuffer_width, h = fb_fbs->framebuffer_height;
    switch (rotation) {
        case 1: {
            return renderer->hit_test(w - 1 - y, x, index);
        }
        case 2: {
            return renderer->hit_test(w - 1 - x, h - 1 - y, index);
        }
        case 3: {
            return renderer->hit_test(y, h - 1 - x, index);
        }
        default: {
            return renderer->hit_test(x, y, index);
        }
    }
}

int main(int argc, char **argv) {
    assert(argc == 3);
    int fd = open(argv[1], O_RDONLY);
    assert(fd >= 0);
    struct stat st;
    assert(fstat(fd, &st) == 0);
    pack_size = st.st_size;
    pack = malloc(pack_size);
    assert(pack != NULL && read(fd, pack, pack_size) == (ssize_t)pack_size);
    close(fd);
    fd = open(argv[2], O_RDONLY);
    assert(fd >= 0 && fstat(fd, &st) == 0);
    header_size = st.st_size;
    header_png = malloc(header_size);
    assert(header_png != NULL && read(fd, header_png, header_size) == (ssize_t)header_size);
    close(fd);
    struct ui_theme theme;
    assert(ui_theme_read(&theme, pack, pack_size));
    assert(!ui_theme_read(&theme, pack, 31));
    assert(!ui_theme_read(&theme, pack, pack_size - 1));
    assert(!ui_theme_read(&theme, pack + 1, pack_size - 1));
    reject_mutation(8, 999);
    reject_mutation(16, UINT32_MAX);
    reject_mutation(32 + 16, UINT32_MAX);
    reject_mutation(32 + 12, UINT32_MAX);
    reject_mutation(64 + 28, 513);
    reject_mutation(64 + 8, 129);
    reject_mutation(64 + 20, 2 * 1024 * 1024 + 1);
    reject_mutation(32 + 8 * 32 + UI_MAX_ROWS * 4, UINT32_MAX);
    assert(ui_theme_read(&theme, pack, pack_size));
    const struct ui_asset *asset = &theme.assets[UI_LABEL];
    struct ui_font *font = ui_font_open(asset->pixels, asset->size, 23);
    assert(font != NULL);
    assert(ui_text_width(font, "WWW", 0) > ui_text_width(font, "iii", 0));
    assert(ui_font_glyph(font, 0xe9)->index != ui_font_glyph(font, '?')->index);
    assert(ui_font_glyph(font, 0x4e00)->index == ui_font_glyph(font, '?')->index);
    uint32_t guarded[102] = {0};
    guarded[0] = guarded[101] = 0xabcdef;
    struct ui_canvas c = {.pixels = &guarded[1], .width = 10, .height = 10};
    ui_rect(&c, -20, -20, 60, 60, 0x80ffffff);
    assert(guarded[1] == 0x808080 && guarded[100] == 0x808080);
    ui_text(&c, font, "Long UTF-8 \xc3\xa9 \xf0\x9f", -4, 8, 0, 0xffffff, 20);
    assert(guarded[0] == 0xabcdef && guarded[101] == 0xabcdef);
    memset(c.pixels, 0, 100 * sizeof(*c.pixels));
    ui_glow(&c, 3, 3, 4, 4, 0, 3, 0x80ffffff);
    assert(c.pixels[4 * 10 + 2] > c.pixels[4 * 10 + 1]);
    assert(c.pixels[4 * 10 + 3] == 0);
    memset(c.pixels, 0, 100 * sizeof(*c.pixels));
    c.clipped = true;
    c.clip_left = c.clip_top = 2; c.clip_right = c.clip_bottom = 8;
    ui_diamond(&c, 5, 5, 6, 0xffffff, 8, 0x80ffffff);
    assert(c.pixels[4 * 10 + 4] == 0xffffff && c.pixels[9 * 10 + 9] == 0);
    ui_glow(&c, -2, -2, 8, 8, 3, 32, 0xffffffff);
    ui_diamond(&c, -1, -1, 128, 0xffffff, 32, 0xffffffff);
    assert(guarded[0] == 0xabcdef && guarded[101] == 0xabcdef);

    ui_font_close(font);
    assert(allocations == 0);
    terminal.double_buffer_flush = flush;
    struct menu_style style = {.branding = "test", .editor_enabled = true, .firmware_setup = true};
    const struct menu_renderer *renderer = &menu_gui_renderer;
    const int sizes[][3] = {{1280,720,0}, {1920,1080,0}, {2560,1440,0}, {1024,768,0},
        {800,600,0}, {1280,720,1}, {1280,720,2}, {1280,720,3}, {720,1280,0}};
    struct menu_row rows[40];
    for (size_t i = 0; i < 40; i++) {
        rows[i] = (struct menu_row){.name = "A very long boot label with accents \xc3\xa9 and unsupported \xe4\xb8\xad", .parent = SIZE_MAX};
    }
    for (size_t mode = 0; mode < sizeof(sizes) / sizeof(sizes[0]); mode++) {
        size_t width = sizes[mode][0], height = sizes[mode][1];
        rotation = sizes[mode][2];
        size_t pitch = width * 4 + 64;
        uint32_t *pixels = calloc(1, pitch * height + 8);
        assert(pixels != NULL);
        pixels[0] = pixels[pitch * height / 4 + 1] = 0xabcdef;
        struct fb_info fb = {.framebuffer_addr = (uintptr_t)(pixels + 1),
            .framebuffer_width = width, .framebuffer_height = height, .framebuffer_pitch = pitch,
            .framebuffer_bpp = 32, .red_mask_size = 8, .green_mask_size = 8, .blue_mask_size = 8,
            .red_mask_shift = mode == 4 ? 0 : 16, .green_mask_shift = 8, .blue_mask_shift = mode == 4 ? 16 : 0};
        fb_fbs = &fb;
        assert(renderer->init(&style));
        struct menu_view view = {.rows = rows, .count = 40, .selected = 39, .config_ready = true,
            .comment = "Long menus scroll; clipped text must stay inside the panel."};
        renderer->draw(&view);
        assert(pointer_width == width && pointer_height == height);
        renderer->present();
        size_t logical_width = rotation & 1 ? height : width;
        size_t logical_height = rotation & 1 ? width : height;
        int scale = logical_width * 65536 / 1920;
        int ox = (logical_width - 1920 * scale / 65536) / 2;
        int oy = (logical_height - ((1080 * scale + 32768) >> 16)) / 2;
        size_t x = ox + ((300 * scale + 32768) >> 16);
        size_t top = oy + ((352 * scale + 32768) >> 16);
        size_t pitch_y = (96 * scale + 32768) >> 16;
        size_t index;
        assert(hit(renderer, x, top + 1, &index) && index == 35);
        assert(hit(renderer, x, top + 4 * pitch_y + 1, &index) && index == 39);
        assert(!hit(renderer, 0, top + 1, &index));
        assert(!hit(renderer, x, top + ((84 * scale + 32768) >> 16), &index));
        assert(!renderer->hit_test(SIZE_MAX, SIZE_MAX, &index));
        pointer_visible = true;
        for (size_t corner = 0; corner < 4; corner++) {
            pointer_x = corner & 1 ? width - 1 : 0;
            pointer_y = corner & 2 ? height - 1 : 0;
            renderer->present();
        }
        pointer_x = SIZE_MAX;
        renderer->present();
        pointer_visible = false;
        renderer->timeout(250);
        renderer->timeout(0);
        renderer->leave();
        assert(pointer_width == 0 && pointer_height == 0);
        assert(!hit(renderer, x, top + 1, &index));
        view.count = 1; view.selected = 0;
        renderer->draw(&view);
        assert(hit(renderer, x, top + 1, &index) && index == 0);
        view.count = 0;
        renderer->draw(&view);
        assert(!hit(renderer, x, top + 1, &index));
        renderer->leave();
        ui_config = "[cursor]\nfile: header.png\nwidth: 128px\nheight: 128px\nhotspot_x: 64px\nhotspot_y: 64px\n";
        assert(renderer->init(&style));
        renderer->draw(&view);
        renderer->present();
        size_t screen_size = pitch * height;
        void *clean = malloc(screen_size);
        assert(clean != NULL);
        memcpy(clean, pixels + 1, screen_size);
        for (size_t corner = 0; corner < 4; corner++) {
            pointer_visible = true;
            pointer_x = corner & 1 ? width - 1 : 0;
            pointer_y = corner & 2 ? height - 1 : 0;
            renderer->present();
            assert(memcmp(clean, pixels + 1, screen_size) != 0);
            renderer->present();
            pointer_visible = false;
            renderer->present();
            assert(memcmp(clean, pixels + 1, screen_size) == 0);
        }
        free(clean);
        renderer->leave();
        ui_config = "[cursor]\nfile: invalid.png\nhotspot_x: 20px\nhotspot_y: 20px\n";
        assert(renderer->init(&style));
        assert(strstr(menu_gui_notice(), "built-in pointer") != NULL);
        renderer->draw(&view);
        pointer_visible = true;
        pointer_x = pointer_y = 0;
        renderer->present();
        assert(pixels[1] == (mode == 4 ? 0x0d0f10 : 0x100f0d));
        pointer_visible = false;
        renderer->present();
        renderer->leave();
        ui_config = "version: 1\ntheme: /theme\n";
        assert(pixels[0] == 0xabcdef && pixels[pitch * height / 4 + 1] == 0xabcdef);
        for (size_t y = 0; y < height; y++) {
            for (size_t pad = width; pad < pitch / 4; pad++) {
                assert(pixels[1 + y * pitch / 4 + pad] == 0);
            }
        }
        if (mode == 0) {
            const char *decoration =
                "version: 1\ntheme: /theme\n[background]\nvisible: no\ncolour: #102030\n"
                "[heading]\nmode: image\nfile: header.png\nx: 20px\ny: 20px\nwidth: 100px\nheight: 100px\n"
                "[divider]\nvisible: no\n[hints]\nvisible: no\n[details]\nvisible: no\n"
                "[menu]\nx: 200px\ny: 200px\nwidth: 400px\nheight: 160px\n"
                "[menu.item]\nheight: 60px\ngap: 20px\n"
                "[menu.item.selected]\nmarker: diamond-left\nmarker_size: 16px\nmarker_colour: #ffffff\n"
                "glow_width: 8px\nglow_opacity: 0.4\n";
            ui_config = decoration;
            assert(renderer->init(&style));
            view.count = 2; view.selected = 0;
            renderer->draw(&view);
            renderer->present();
            assert(pixels[1 + 25 * pitch / 4 + 25] == 0x102030);
            assert(pixels[1 + 55 * pitch / 4 + 25] == 0x806040);
            assert(pixels[1 + 230 * pitch / 4 + 196] == 0xffffff);
            assert(!hit(renderer, 196, 230, &index));
            assert(hit(renderer, 201, 230, &index) && index == 0);
            view.selected = 1;
            renderer->draw(&view);
            renderer->present();
            assert(pixels[1 + 230 * pitch / 4 + 196] == 0x102030);
            assert(pixels[1 + 310 * pitch / 4 + 196] == 0xffffff);
            char right[2048];
            strcpy(right, decoration);
            char *side = strstr(right, "diamond-left");
            assert(side != NULL);
            memmove(side + 13, side + 12, strlen(side + 12) + 1);
            memcpy(side, "diamond-right", 13);
            ui_config = right;
            assert(renderer->init(&style));
            renderer->draw(&view);
            renderer->present();
            assert(pixels[1 + 310 * pitch / 4 + 603] == 0xffffff);
            assert(!hit(renderer, 603, 310, &index));
            strcpy(right + strlen(right), "[heading]\nfit: stretch\n");
            assert(renderer->init(&style));
            renderer->draw(&view);
            renderer->present();
            assert(pixels[1 + 25 * pitch / 4 + 25] == 0x806040);
            ui_config = "[details]\nfont_size: 25px\nheight: 80px\ny: 10px\noverflow: wrap\n";
            assert(renderer->init(&style));
            renderer->draw(&view);
            renderer->present();
            ui_config = "[details]\nfont: invalid.ttf\n";
            assert(!renderer->init(&style) && allocations == 0);
            assert(strstr(menu_gui_notice(), "font") != NULL);
            ui_config = "[heading]\nmode: image\nfile: invalid.png\nheight: 100px\n";
            assert(!renderer->init(&style) && allocations == 0);
            assert(strstr(menu_gui_notice(), "heading PNG") != NULL);
            ui_config = "[heading]\nmode: image\nfile: invalid.png\nheight: 100px\nvisible: no\n";
            assert(renderer->init(&style));
            ui_config = "version: 1\ntheme: /theme\n";
        }
        missing = true;
        assert(!renderer->init(&style) && allocations == 0);
        missing = false;
        uint32_t font_offset;
        memcpy(&font_offset, pack + 64 + 16, 4);
        uint8_t saved[12];
        memcpy(saved, pack + font_offset, sizeof(saved));
        memset(pack + font_offset, 0, sizeof(saved));
        assert(!renderer->init(&style) && allocations == 0);
        memcpy(pack + font_offset, saved, sizeof(saved));
        free(pixels);
    }
    free(pack);
    free(header_png);
    const char message[] = "GUI parser, compositor, layout, pointer bounds and lifecycle checks passed.\n";
    assert(write(1, message, sizeof(message) - 1) == sizeof(message) - 1);
    return 0;
}
