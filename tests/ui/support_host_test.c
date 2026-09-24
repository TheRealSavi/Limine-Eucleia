#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ui/support.h>

static size_t allocations;
void *ext_mem_alloc(uint64_t size) {
    void *ptr = calloc(1, size);
    assert(ptr != NULL);
    allocations++;
    return ptr;
}
void pmm_free(void *ptr, uint64_t size) {
    assert(ptr != NULL && size != 0 && allocations != 0);
    allocations--;
    free(ptr);
}

static struct ui_settings settings;
static struct ui_document document;
static struct ui_theme theme;
static char input[UI_CONFIG_MAX_SIZE + 1];
static void parse(const char *text) {
    strcpy(input, text);
    assert(ui_document_read(&document, input, strlen(input)));
    assert(ui_settings_read(&settings, &document, &theme, 1280, 720));
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file != NULL && fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    rewind(file);
    void *data = malloc(size);
    assert(data != NULL && fread(data, 1, size, file) == (size_t)size);
    fclose(file);
    assert(ui_theme_read(&theme, data, size));
    struct ui_font *font = ui_font_open(theme.assets[UI_HINT].pixels, theme.assets[UI_HINT].size, 20);
    assert(font != NULL);
    uint32_t *pixels = calloc(1280 * 720 + 2, 4), *expected = calloc(1280 * 720, 4);
    assert(pixels != NULL && expected != NULL);
    pixels[0] = pixels[1280 * 720 + 1] = 0xabcdef;
    struct ui_canvas c = {.pixels = pixels + 1, .width = 1280, .height = 720};
    const char *labels[UI_HINT_COUNT] = {"Select", "Boot", "Edit", "New entry", "Firmware", "Shell"};
    parse("[hints]\nlayout: grid\nx: 10px\ny: 10px\nwidth: 800px\nheight: 240px\n"
        "padding: 10px\nitem_height: 50px\ngap: 10px\nfont_size: 20px\nalign: left\n"
        "[hints.key]\nwidth: 80px\nfill_colour: #123456\nfill_opacity: 1\n"
        "border_width: 1px\nborder_colour: #abcdef\n");
    ui_draw_hints(&c, font, &settings, labels);
    assert(c.pixels[20 * 1280 + 20] == 0xabcdef);
    assert(c.pixels[22 * 1280 + 22] == 0x123456);
    assert(c.pixels[22 * 1280 + 417] == 0x123456);
    assert(c.pixels[82 * 1280 + 22] == 0x123456);
    memset(c.pixels, 0, 1280 * 720 * 4);
    labels[UI_HINT_SELECT] = labels[UI_HINT_EDIT] = NULL;
    settings.hint_items[UI_HINT_FIRMWARE].visible = false;
    ui_draw_hints(&c, font, &settings, labels);
    assert(c.pixels[82 * 1280 + 22] == 0x123456);
    assert(c.pixels[82 * 1280 + 417] == 0);
    assert(c.pixels[142 * 1280 + 22] == 0);
    for (int layout = UI_HINT_HORIZONTAL; layout <= UI_HINT_CUSTOM; layout++) {
        settings.hint_layout = layout;
        settings.hints.height = 600;
        for (int position = 0; position < 3; position++) {
            settings.key_position = position;
            ui_draw_hints(&c, font, &settings, labels);
        }
    }
    parse("[details]\nx: 20px\ny: 20px\nwidth: 100px\nheight: 60px\nfont_size: 20px\n"
        "letter_spacing: 0px\ncolour: #ffffff\noverflow: wrap\nline_gap: 0px\n");
    memset(c.pixels, 0, 1280 * 720 * 4);
    ui_draw_details(&c, font, &settings, "First\nSecond\nThird\nHidden", NULL);
    struct ui_canvas reference = {.pixels = expected, .width = 1280, .height = 720};
    ui_text(&reference, font, "First", 20, 35, 0, 0xffffff, 100);
    ui_text(&reference, font, "Second", 20, 55, 0, 0xffffff, 100);
    ui_text(&reference, font, "Third\xe2\x80\xa6", 20, 75, 0, 0xffffff, 100);
    assert(memcmp(expected, c.pixels, 1280 * 720 * 4) == 0);
    memset(c.pixels, 0, 1280 * 720 * 4);
    memset(expected, 0, 1280 * 720 * 4);
    settings.details.width = ui_text_width(font, "First Second", 0);
    ui_draw_details(&c, font, &settings, "First Second Third", NULL);
    ui_text(&reference, font, "First Second", 20, 35, 0, 0xffffff, settings.details.width);
    ui_text(&reference, font, "Third", 20, 55, 0, 0xffffff, settings.details.width);
    assert(memcmp(expected, c.pixels, 1280 * 720 * 4) == 0);
    settings.detail_selection = true;
    settings.detail_panel.fill = 0x123456;
    settings.detail_panel.opacity = 255;
    struct ui_rect selection = {1250, 690, 100, 50};
    ui_draw_details(&c, font, &settings, "Unicode \xc3\xa9 \xf0\x9f\x98\x80 and averylongunbrokenword", &selection);
    assert(c.pixels[710 * 1280 + 1270] == 0x123456);
    char long_text[8193];
    memset(long_text, 'a', sizeof(long_text) - 1);
    long_text[8192] = 0;
    ui_draw_details(&c, font, &settings, long_text, &selection);
    assert(pixels[0] == 0xabcdef && pixels[1280 * 720 + 1] == 0xabcdef);
    ui_font_close(font);
    assert(allocations == 0);
    free(pixels); free(expected); free(data);
    puts("Hint layout, availability, keycap borders, wrapped text, ellipsis and edge clipping passed.");
    return 0;
}
