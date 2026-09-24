#include <ui/font.h>
#include <lib/libc.h>
#include <mm/pmm.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#ifndef UI_FONT_BUDGET
#define UI_FONT_BUDGET (4 * 1024 * 1024)
#endif
#define FONT_BUDGET UI_FONT_BUDGET
#define GLYPH_CACHE_SIZE 128

struct cached_glyph {
    uint32_t codepoint;
    bool valid;
    struct ui_glyph glyph;
};

struct font_block {
    size_t size;
    struct font_block *next;
    bool free;
} __attribute__((aligned(16)));

struct ui_font {
    struct FT_MemoryRec_ memory;
    FT_Library library;
    FT_Face face;
    struct font_block *arena;
    ui_ft_jump_buffer allocation_failure;
    bool failed;
    struct cached_glyph cache[GLYPH_CACHE_SIZE];
};

static void *font_alloc(FT_Memory memory, long size) {
    struct ui_font *font = memory->user;
    if (size > 0 && size <= FONT_BUDGET) {
        size_t bytes = ((size_t)size + 15) & ~(size_t)15;
        for (struct font_block *block = font->arena; block != NULL; block = block->next) {
            if (!block->free || block->size < bytes) {
                continue;
            }
            if (block->size >= bytes + sizeof(*block) + 16) {
                struct font_block *next = (void *)((uint8_t *)(block + 1) + bytes);
                *next = (struct font_block){.size = block->size - bytes - sizeof(*block),
                    .next = block->next, .free = true};
                block->next = next;
                block->size = bytes;
            }
            block->free = false;
            return block + 1;
        }
    }
    // Abort the face as a unit: some auto-hinter paths cannot recover from OOM.
    font->failed = true;
    ft_longjmp(font->allocation_failure, 1);
}

static void font_free(FT_Memory memory, void *ptr) {
    if (ptr == NULL) {
        return;
    }
    struct ui_font *font = memory->user;
    struct font_block *block = (struct font_block *)ptr - 1;
    block->free = true;
    for (block = font->arena; block != NULL && block->next != NULL;) {
        if (block->free && block->next->free) {
            block->size += sizeof(*block) + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
}

static void *font_realloc(FT_Memory memory, long old_size, long size, void *ptr) {
    void *result = font_alloc(memory, size);
    if (ptr != NULL && old_size > 0) {
        memcpy(result, ptr, old_size < size ? old_size : size);
    }
    font_free(memory, ptr);
    return result;
}

// Faces are memory-only; no host filesystem or C allocator is available.
FT_Error FT_Stream_Open(FT_Stream stream, const char *path) {
    (void)stream; (void)path;
    return FT_Err_Cannot_Open_Resource;
}

FT_Memory FT_New_Memory(void) {
    return NULL;
}

void FT_Done_Memory(FT_Memory memory) {
    (void)memory;
}

char *ui_ft_strstr(const char *text, const char *needle) {
    size_t length = strlen(needle);
    for (; *text != '\0'; text++) {
        if (strncmp(text, needle, length) == 0) {
            return (char *)text;
        }
    }
    return length == 0 ? (char *)text : NULL;
}

static void swap(uint8_t *a, uint8_t *b, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint8_t value = a[i];
        a[i] = b[i];
        b[i] = value;
    }
}

static void sift(uint8_t *base, size_t root, size_t count, size_t size,
    int (*compare)(const void *, const void *)) {
    while (root < count / 2) {
        size_t child = root * 2 + 1;
        if (child + 1 < count && compare(base + child * size, base + (child + 1) * size) < 0) {
            child++;
        }
        if (compare(base + root * size, base + child * size) >= 0) {
            break;
        }
        swap(base + root * size, base + child * size, size);
        root = child;
    }
}

void ui_ft_qsort(void *base, size_t count, size_t size,
    int (*compare)(const void *, const void *)) {
    if (size == 0 || count < 2 || count > SIZE_MAX / size) {
        return;
    }
    uint8_t *p = base;
    for (size_t i = count / 2; i != 0; i--) {
        sift(p, i - 1, count, size, compare);
    }
    for (size_t i = count - 1; i != 0; i--) {
        swap(p, p + i * size, size);
        sift(p, 0, i, size, compare);
    }
}

void ui_font_close(struct ui_font *font) {
    if (font == NULL) {
        return;
    }
    // All FreeType and cached-mask allocations belong to this arena.
    pmm_free(font->arena, FONT_BUDGET);
    pmm_free(font, sizeof(*font));
}

struct ui_font *ui_font_open(const void *data, size_t size, unsigned pixels) {
    if (data == NULL || size < 12 || size > 2 * 1024 * 1024 || pixels < 4 || pixels > 256) {
        return NULL;
    }
    struct ui_font *font = ext_mem_alloc(sizeof(*font));
    memset(font, 0, sizeof(*font));
    font->arena = ext_mem_alloc(FONT_BUDGET);
    *font->arena = (struct font_block){.size = FONT_BUDGET - sizeof(*font->arena), .free = true};
    if (ft_setjmp(font->allocation_failure) != 0) {
        ui_font_close(font);
        return NULL;
    }
    font->memory = (struct FT_MemoryRec_){.user = font,
        .alloc = font_alloc, .free = font_free, .realloc = font_realloc};
    if (FT_New_Library(&font->memory, &font->library) != 0) {
        ui_font_close(font);
        return NULL;
    }
    FT_Add_Default_Modules(font->library);
    if (FT_New_Memory_Face(font->library, data, size, 0, &font->face) != 0
     || !FT_IS_SCALABLE(font->face)
     || FT_Select_Charmap(font->face, FT_ENCODING_UNICODE) != 0
     || FT_Set_Pixel_Sizes(font->face, 0, pixels) != 0
     || FT_Get_Char_Index(font->face, '?') == 0
     || ui_font_glyph(font, '?') == NULL || font->failed) {
        ui_font_close(font);
        return NULL;
    }
    for (uint32_t codepoint = 32; codepoint < 127; codepoint++) {
        if (ui_font_glyph(font, codepoint) == NULL || font->failed) {
            ui_font_close(font);
            return NULL;
        }
    }
    if (ui_font_glyph(font, 0x2026) == NULL || font->failed) {
        ui_font_close(font);
        return NULL;
    }
    return font;
}

const struct ui_glyph *ui_font_glyph(struct ui_font *font, uint32_t codepoint) {
    if (font->failed || ft_setjmp(font->allocation_failure) != 0) {
        return NULL;
    }
    unsigned index = FT_Get_Char_Index(font->face, codepoint);
    if (index == 0 && codepoint != '?') {
        return ui_font_glyph(font, '?');
    }
    struct cached_glyph *entry = &font->cache[codepoint % GLYPH_CACHE_SIZE];
    if (entry->valid && entry->codepoint == codepoint) {
        return &entry->glyph;
    }
    FT_GlyphSlot slot = font->face->glyph;
    if (FT_Load_Glyph(font->face, index,
        FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT) != 0
     || slot->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY
     || slot->bitmap.width > 256 || slot->bitmap.rows > 256
     || slot->bitmap.pitch < (int)slot->bitmap.width
     || slot->advance.x < 0 || slot->advance.x > 512 * 64
     || slot->bitmap_left < -512 || slot->bitmap_left > 512
     || slot->bitmap_top < -512 || slot->bitmap_top > 512) {
        font->failed = true;
        return NULL;
    }
    font_free(&font->memory, (void *)entry->glyph.pixels);
    *entry = (struct cached_glyph){0};
    size_t bytes = (size_t)slot->bitmap.width * slot->bitmap.rows;
    uint8_t *pixels = NULL;
    if (bytes != 0) {
        pixels = font_alloc(&font->memory, bytes);
        for (unsigned y = 0; y < slot->bitmap.rows; y++) {
            memcpy(pixels + y * slot->bitmap.width,
                slot->bitmap.buffer + y * slot->bitmap.pitch, slot->bitmap.width);
        }
    }
    *entry = (struct cached_glyph){.codepoint = codepoint, .valid = true,
        .glyph = {.index = index, .width = slot->bitmap.width, .height = slot->bitmap.rows,
            .left = slot->bitmap_left, .top = slot->bitmap_top,
            .advance = slot->advance.x, .pixels = pixels}};
    return &entry->glyph;
}

int ui_font_kerning(struct ui_font *font, unsigned left, unsigned right) {
    FT_Vector delta = {0};
    if (left != 0 && right != 0) {
        FT_Get_Kerning(font->face, left, right, FT_KERNING_DEFAULT, &delta);
    }
    return delta.x < -512 * 64 || delta.x > 512 * 64 ? 0 : delta.x;
}

bool ui_font_failed(const struct ui_font *font) {
    return font->failed;
}
