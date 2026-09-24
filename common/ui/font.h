#ifndef UI__FONT_H__
#define UI__FONT_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ui_font;

struct ui_glyph {
    unsigned index, width, height;
    int left, top, advance;
    const uint8_t *pixels;
};

// Font data must outlive the face. Metrics and masks are at the final pixel size.
struct ui_font *ui_font_open(const void *data, size_t size, unsigned pixels);
void ui_font_close(struct ui_font *font);
// The returned glyph is borrowed until the next lookup on this face.
const struct ui_glyph *ui_font_glyph(struct ui_font *font, uint32_t codepoint);
int ui_font_kerning(struct ui_font *font, unsigned left, unsigned right);
bool ui_font_failed(const struct ui_font *font);

#endif
