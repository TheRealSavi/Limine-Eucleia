#ifndef UI__DISPLAY_H__
#define UI__DISPLAY_H__

#include <lib/fb.h>
#include <ui/canvas.h>

struct ui_display {
    struct fb_info *fb;
    int width, height;
    unsigned rotation;
};

// Rotation is the number of clockwise quarter turns, as in gterm.
bool ui_display_init(struct ui_display *display, struct fb_info *fb, unsigned rotation);
bool ui_display_position(const struct ui_display *display, size_t x, size_t y,
    int *logical_x, int *logical_y);
void ui_display_blit(const struct ui_display *display, const struct ui_canvas *canvas,
    int x, int y, int width, int height);

#endif
