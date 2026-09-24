#ifndef UI__CANVAS_H__
#define UI__CANVAS_H__

#include <ui/theme.h>
#include <ui/font.h>

struct ui_canvas {
    uint32_t *pixels;
    int width, height;
    bool clipped;
    int clip_left, clip_top, clip_right, clip_bottom;
};

void ui_rect(struct ui_canvas *c, int x, int y, int width, int height, uint32_t argb);
void ui_surface(struct ui_canvas *c, int x, int y, int width, int height,
    uint32_t fill, uint32_t border, int border_width, int radius);
void ui_glow(struct ui_canvas *c, int x, int y, int width, int height,
    int radius, int spread, uint32_t argb);
void ui_diamond(struct ui_canvas *c, int x, int y, int size, uint32_t colour,
    int spread, uint32_t glow);
void ui_image(struct ui_canvas *c, const struct ui_asset *image,
    int x, int y, int width, int height);
int ui_text_width(struct ui_font *font, const char *text, int tracking);
void ui_text(struct ui_canvas *c, struct ui_font *font, const char *text,
    int x, int baseline, int tracking, uint32_t colour, int max_width);

#endif
