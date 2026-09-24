#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ui/canvas.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

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

static void *reference_alloc(FT_Memory memory, long size) {
    (void)memory;
    return malloc(size);
}

static void reference_free(FT_Memory memory, void *ptr) {
    (void)memory;
    free(ptr);
}

static void *reference_realloc(FT_Memory memory, long old_size, long size, void *ptr) {
    (void)memory; (void)old_size;
    return realloc(ptr, size);
}

static void *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    *size = ftell(file);
    rewind(file);
    void *data = malloc(*size);
    assert(data != NULL && fread(data, 1, *size, file) == *size);
    fclose(file);
    return data;
}

static int reference_text(FT_Face face, uint32_t *pixels, const char *text, int tracking) {
    int advance = 0;
    unsigned previous = 0;
    for (; *text; text++) {
        unsigned index = FT_Get_Char_Index(face, (unsigned char)*text);
        FT_Vector kern = {0};
        assert(!FT_Get_Kerning(face, previous, index, FT_KERNING_DEFAULT, &kern));
        advance += kern.x;
        assert(!FT_Load_Glyph(face, index,
            FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT));
        FT_GlyphSlot g = face->glyph;
        int left = 10 + (advance + 32) / 64 + g->bitmap_left;
        int top = 200 - g->bitmap_top;
        for (unsigned y = 0; y < g->bitmap.rows; y++) {
            for (unsigned x = 0; x < g->bitmap.width; x++) {
                assert(left + (int)x >= 0 && left + x < 2048 && top + (int)y >= 0 && top + y < 320);
                if (left + (int)x < 10) {
                    continue;
                }
                unsigned alpha = g->bitmap.buffer[y * g->bitmap.pitch + x];
                uint32_t *dst = &pixels[(top + y) * 2048 + left + x];
                unsigned level = ((*dst & 255) * (255 - alpha) + 255 * alpha + 127) / 255;
                *dst = level * 0x010101;
            }
        }
        advance += g->advance.x + tracking;
        previous = index;
    }
    return (advance - tracking + 32) / 64;
}

static void write_image(const char *path, const uint32_t *pixels) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    fprintf(file, "P6\n2048 320\n255\n");
    for (size_t i = 0; i < 2048 * 320; i++) {
        unsigned char rgb[] = {pixels[i] >> 16, pixels[i] >> 8, pixels[i]};
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    fclose(file);
}

int main(void) {
    struct FT_MemoryRec_ memory = {.alloc = reference_alloc, .free = reference_free,
        .realloc = reference_realloc};
    FT_Library reference;
    assert(!FT_New_Library(&memory, &reference));
    FT_Add_Default_Modules(reference);
    const char *paths[] = {
        "work/fonts/label.ttf",
        "work/fonts/heading.ttf"
    };
    const unsigned sizes[] = {14, 23, 34, 45, 48, 72, 96};
    const char *strings[] = {"Limine protocol test", "EUCLEIA", "HHH III xxx ppp", "AV To WA"};
    uint32_t *actual = calloc(2048 * 320, 4), *expected = calloc(2048 * 320, 4);
    assert(actual != NULL && expected != NULL);
    struct ui_canvas canvas = {.pixels = actual, .width = 2048, .height = 320};
    size_t comparisons = 0;
    for (size_t f = 0; f < 2; f++) {
        size_t size;
        uint8_t *data = read_file(paths[f], &size);
        FT_Face face;
        assert(!FT_New_Memory_Face(reference, data, size, 0, &face));
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            struct ui_font *font = ui_font_open(data, size, sizes[s]);
            assert(font != NULL);
            assert(!FT_Set_Pixel_Sizes(face, 0, sizes[s]));
            for (size_t text = 0; text < 4; text++) {
                for (int tracking = 0; tracking <= 43; tracking += 43) {
                    memset(actual, 0, 2048 * 320 * 4);
                    memset(expected, 0, 2048 * 320 * 4);
                    ui_text(&canvas, font, strings[text], 10, 200, tracking, 0xffffff, 2038);
                    int width = reference_text(face, expected, strings[text], tracking);
                    assert(ui_text_width(font, strings[text], tracking) == width);
                    assert(memcmp(actual, expected, 2048 * 320 * 4) == 0);
                    comparisons++;
                    if (f == 0 && sizes[s] == 23 && text == 0 && tracking == 0) {
                        write_image("work/fonts/native-label.ppm", actual);
                    }
                }
            }
            memset(actual, 0, 2048 * 320 * 4);
            ui_text(&canvas, font, "AV To a very long clipped label", 10, 200, 43, 0xffffff, 30);
            for (size_t y = 0; y < 320; y++) {
                for (size_t x = 0; x < 2048; x++) {
                    if (x < 10 || x >= 40) {
                        assert(actual[y * 2048 + x] == 0);
                    }
                }
            }
            assert(!ui_font_failed(font));
            ui_font_close(font);
            assert(allocations == 0);
        }
        if (f == 0) {
            struct ui_font *font = ui_font_open(data, size, 72);
            assert(font != NULL);
            assert(ui_text_width(font, "AV", 0) < ui_text_width(font, "A", 0) + ui_text_width(font, "V", 0));
            unsigned question = ui_font_glyph(font, '?')->index;
            assert(ui_font_glyph(font, 0x10ffff)->index == question);
            assert(ui_font_glyph(font, 0x3a9)->index != question);
            assert(ui_text_width(font, "\xf0\x9f", 0) == ui_text_width(font, "?", 0));
            for (unsigned codepoint = 32; codepoint < 1024; codepoint++) {
                assert(ui_font_glyph(font, codepoint) != NULL);
            }
            unsigned index = ui_font_glyph(font, 'A')->index;
            assert(index != question && !ui_font_failed(font));
            ui_font_close(font);
            assert(allocations == 0);
            // Exercise malformed SFNT directories and the validator's non-local exits.
            for (size_t i = 0; i < 32; i++) {
                size_t offset = 12 + i * 16;
                uint8_t saved[16];
                memcpy(saved, data + offset, 16);
                memset(data + offset, 0xff, 16);
                font = ui_font_open(data, size, 23);
                if (font != NULL) {
                    ui_font_glyph(font, 'A');
                    ui_font_close(font);
                }
                memcpy(data + offset, saved, 16);
                assert(allocations == 0);
            }
        }
        FT_Done_Face(face);
        free(data);
    }
    uint8_t invalid[128] = {0};
    assert(ui_font_open(invalid, sizeof(invalid), 23) == NULL && allocations == 0);
    assert(ui_font_open(NULL, 128, 23) == NULL);
    assert(ui_font_open(invalid, sizeof(invalid), 257) == NULL);
    FT_Done_Library(reference);
    free(actual); free(expected);
    printf("%zu native-size FreeType pixel/advance comparisons passed; kerning, clipping, Unicode, cache eviction and malformed fonts passed.\n", comparisons);
    return 0;
}
