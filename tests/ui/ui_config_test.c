#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ui/config.h>

static struct ui_theme theme;
static struct ui_document document;
static struct ui_settings settings;
static char input[UI_CONFIG_MAX_SIZE + 1];

static bool parse(const char *text) {
    strcpy(input, text);
    return ui_document_read(&document, input, strlen(input))
        && ui_settings_read(&settings, &document, &theme, 1280, 720);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size > 0 && fseek(f, 0, SEEK_SET) == 0);
    void *pack = malloc(size);
    assert(pack != NULL && fread(pack, 1, size, f) == (size_t)size);
    fclose(f);
    assert(ui_theme_read(&theme, pack, size));
    assert(parse("version: 1\n"));
    assert(settings.menu.x == 181 && settings.menu.y == 235 && settings.max_rows == 5);
    assert(parse("version: 1\n[menu]\nx: 92%\ny: 50%\nanchor: right-center\nwidth: 48%\nheight: 48%\npadding: 16px\n"
        "[menu.item]\nheight: 64px\ngap: 12px\n[menu.item.label]\nfont_size: 26px\nletter_spacing: 0.5px\n"
        "[menu.item.selected]\nfill_colour: #abcdef\nfill_opacity: 0.3\ncorner_radius: 8px\n"
        "[entry \"Group/Child\"]\nicon: icons/a.png\n"));
    assert(settings.menu.x == 564 && settings.menu.y == 187 && settings.menu.width == 614);
    assert(settings.max_rows == 4 && settings.text[1].size == 26 && settings.text[1].tracking == 32);
    assert(settings.rows[1].fill == 0xabcdef && settings.rows[1].opacity == 77 && settings.rows[1].radius == 8);
    assert(settings.icon_count == 1 && strcmp(settings.icons[0].entry, "Group/Child") == 0);
    assert(parse("[heading]\nmode: image\nfile: components/header.png\nx: 10%\ny: 20px\nwidth: 40%\nheight: 180u\n"
        "[menu.item.selected]\nmarker: diamond-right\nmarker_size: 18u\nglow_width: 12u\nglow_opacity: 0.4\n"));
    assert(settings.heading_image && !settings.heading_stretch && settings.heading.height == 120);
    assert(settings.heading.x == 128 && settings.heading.y == 20 && settings.heading.width == 512);
    assert(strcmp(settings.heading_file, "components/header.png") == 0);
    assert(settings.marker_side == 2 && settings.marker_size == 12 && settings.glow_width == 8);
    assert(settings.glow_opacity == 102 && settings.marker_colour == settings.rows[1].border);
    assert(parse("[heading]\nmode: text\nfile: unused.png\n[menu.item.selected]\nmarker: none\n"));
    assert(!settings.heading_image && settings.marker_side == 0 && settings.glow_width == 0);
    assert(parse("[heading]\nmode: image\nfile: header.png\nheight: 20%\nfit: stretch\n"
        "[menu.item.selected]\nmarker: diamond-left\nmarker_colour: #abcdef\nglow_colour: #123456\n"));
    assert(settings.heading_stretch && settings.heading.height == 144 && settings.marker_side == 1);
    assert(settings.marker_colour == 0xabcdef && settings.glow_colour == 0x123456);
    assert(parse("[cursor]\nfile: cursor.png\nwidth: 48u\nheight: 60u\nhotspot_x: 3px\nhotspot_y: 6u\n"));
    assert(strcmp(settings.cursor_file, "cursor.png") == 0);
    assert(settings.cursor_width == 32 && settings.cursor_height == 40);
    assert(settings.cursor_hotspot_x == 3 && settings.cursor_hotspot_y == 4);
    assert(parse("[hints]\nlayout: grid\nx: 50%\ny: 20px\nwidth: 600px\nheight: 150px\nanchor: top-center\n"
        "columns: 3\nitem_height: 40px\ngap: 10px\nfont_size: 16px\nfont: hints.ttf\n"
        "order: enter, select, edit, blank, shell, firmware\n[hints.key]\nposition: after\ncolour: #abcdef\n"
        "[hints.edit]\nlabel: Change options\nvisible: no\n"
        "[details]\nrelative_to: selection\nx: -120px\ny: 2px\nwidth: 100px\nheight: 80px\n"
        "overflow: wrap\npadding: 4px\nfont_size: 18px\ncolour: #123456\n"));
    assert(settings.hint_layout == UI_HINT_GRID && settings.hints.x == 340 && settings.hint_columns == 3);
    assert(settings.hint_order[0] == UI_HINT_ENTER && settings.hint_order[4] == UI_HINT_SHELL);
    assert(settings.key_position == 1 && settings.key_colour == 0xabcdef && !settings.hint_items[UI_HINT_EDIT].visible);
    assert(strcmp(settings.hint_items[UI_HINT_EDIT].label, "Change options") == 0);
    assert(settings.detail_selection && settings.detail_wrap && settings.details.x == -120);
    assert(settings.text[3].size == 18 && settings.text[3].colour == 0x123456 && settings.text[2].size == 16);
    assert(strcmp(settings.text[3].file, "hints.ttf") == 0);
    assert(parse("[hints]\nlayout: custom\nx: 20px\ny: 20px\nwidth: 600px\nheight: 400px\n"
        "[hints.enter]\nx: 50%\ny: 0px\nwidth: 50%\nheight: 40px\n"));
    assert(settings.hint_items[UI_HINT_ENTER].box.x == 300 && settings.hint_items[UI_HINT_ENTER].box.width == 300);
    const char *bad[] = {
        "[hints]\nlayout: circle", "[hints]\ncolumns: 7", "[hints]\ncolumns: 2.5",
        "[hints]\norder: enter,select,edit,blank,shell,shell", "[hints]\norder: enter,select",
        "[hints]\norder: enter,select,edit,blank,shell,firmware,", "[hints]\ngap: -1px",
        "[hints.key]\nposition: underneath", "[hints.key]\nwidth: 2px\npadding: 2px",
        "[hints]\nlayout: vertical\ny: 10px\nheight: 40px",
        "[hints]\nlayout: custom\ny: 10px\nheight: 300px\n[hints.enter]\nx: -1px",
        "[hints]\nlayout: grid\ny: 10px\nheight: 300px\n[hints.key]\nposition: above",
        "[details]\noverflow: wrap", "[details]\nrelative_to: row", "[details]\nfont_size: 2px",
        "[details]\nheight: 20px\npadding: 30px", "[details]\nline_gap: -1px",
        "[details]\nrelative_to: selection\nwidth: -1px", "[details]\nanchor: unknown",
        "version: 2", "enabled: maybe", "timeout: 0", "[made-up]\nx: 3px", "[menu\nx: 0px",
        "title: A\ntitle: B", "[menu]\nwidth: 2", "[menu]\nwidth: 999999999999px",
        "[menu]\nx: -1px", "[menu]\nwidth: 1%", "[menu]\nheight: 1px", "[menu]\npadding: 999px",
        "[menu]\nanchor: nowhere", "[menu]\nmin_width: 500px\nmax_width: 200px",
        "[menu.item]\nheight: 0px", "[menu.item]\ngap: -2px", "[menu.item]\nfill_opacity: 1.1",
        "[menu.item]\nfill_colour: #abcxyz", "[menu.item.label]\nfont_size: 999px",
        "[menu.item.label]\noverflow: wrap", "[menu.item.label]\nletter_spacing: 99px",
        "[menu.item.label]\nbaseline: 10000px", "[heading]\nx: 100%", "[entry \"A\"]\nprotocol: linux",
        "[menu.item.selected]\nfill_colour: #12", "[menu]\nx: 0.1234px",
        "[heading]\nmode: unknown", "[heading]\nmode: image\nfile: header.png",
        "[heading]\nmode: image\nheight: 100px", "[heading]\nmode: image\nfile:\nheight: 100px",
        "[heading]\nmode: image\nfile: header.png\nheight: -1px", "[heading]\nheight: 2000px",
        "[heading]\nfit: cover", "[menu.item.selected]\nmarker: triangle",
        "[menu.item.selected]\nmarker: diamond-left\nmarker_size: 100px",
        "[menu.item.selected]\nmarker_size: -1px", "[menu.item.selected]\nglow_width: 33px",
        "[menu.item.selected]\nglow_opacity: 1.1", "[menu.item.selected]\nglow_width: -1px",
        "[cursor]\nwidth: 0px", "[cursor]\nheight: 129px", "[cursor]\nwidth: 50%",
        "[cursor]\nhotspot_x: -1px", "[cursor]\nwidth: 24px\nhotspot_x: 24px",
        "[cursor]\nheight: 32px\nhotspot_y: 32px"
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(!parse(bad[i]) && document.error != NULL);
    }
    assert(!parse("version: 1\n[menu]\nwidth: bad\n") && document.error_line == 3);
    memcpy(input, "title: a\0b", 10);
    assert(!ui_document_read(&document, input, 10));
    char path[64];
    assert(ui_config_path(path, sizeof(path), "/boot/limine/limine.conf", "eucleia.conf"));
    assert(strcmp(path, "/boot/limine/eucleia.conf") == 0);
    assert(ui_config_path(path, sizeof(path), "/boot/limine/eucleia.conf", "icons/custom.png"));
    assert(strcmp(path, "/boot/limine/icons/custom.png") == 0);
    assert(ui_config_path(path, sizeof(path), "/a/eucleia.conf", "/icons/custom.png"));
    assert(strcmp(path, "/icons/custom.png") == 0);
    assert(!ui_config_path(path, 8, "/boot/limine/eucleia.conf", "icons/custom.png"));
    assert(!ui_config_path(path, sizeof(path), NULL, "eucleia.conf"));
    free(pack);
    puts("UI configuration, geometry, validation and relative-path checks passed.");
    return 0;
}
