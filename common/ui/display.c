#include <ui/display.h>

bool ui_display_init(struct ui_display *display, struct fb_info *fb, unsigned rotation) {
    uint64_t width = fb->framebuffer_width, height = fb->framebuffer_height;
    if (rotation > 3 || fb->framebuffer_bpp != 32 || width < 600 || height < 600
     || width > 4096 || height > 4096 || width * height > 4096 * 2160
     || (width < 800 && height < 800) || fb->framebuffer_addr == 0
     || fb->framebuffer_pitch < width * 4 || fb->framebuffer_pitch > 65536
     || (fb->framebuffer_pitch & 3) != 0
     || fb->red_mask_size != 8 || fb->green_mask_size != 8 || fb->blue_mask_size != 8
     || fb->green_mask_shift != 8
     || !((fb->red_mask_shift == 16 && fb->blue_mask_shift == 0)
       || (fb->red_mask_shift == 0 && fb->blue_mask_shift == 16))) {
        return false;
    }
    display->fb = fb;
    display->rotation = rotation;
    display->width = rotation & 1 ? height : width;
    display->height = rotation & 1 ? width : height;
    return true;
}

bool ui_display_position(const struct ui_display *display, size_t x, size_t y,
    int *logical_x, int *logical_y) {
    if (x >= display->fb->framebuffer_width || y >= display->fb->framebuffer_height) {
        return false;
    }
    switch (display->rotation) {
        case 1: {
            *logical_x = y;
            *logical_y = display->height - 1 - x;
            break;
        }
        case 2: {
            *logical_x = display->width - 1 - x;
            *logical_y = display->height - 1 - y;
            break;
        }
        case 3: {
            *logical_x = display->width - 1 - y;
            *logical_y = x;
            break;
        }
        default: {
            *logical_x = x;
            *logical_y = y;
            break;
        }
    }
    return true;
}

void ui_display_blit(const struct ui_display *display, const struct ui_canvas *canvas,
    int x, int y, int width, int height) {
    if (canvas->width != display->width || canvas->height != display->height
     || width <= 0 || height <= 0) {
        return;
    }
    int64_t right = (int64_t)x + width, bottom = (int64_t)y + height;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = right > canvas->width ? canvas->width : right;
    int y1 = bottom > canvas->height ? canvas->height : bottom;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    int left = x0, top = y0, end_x = x1, end_y = y1;
    switch (display->rotation) {
        case 1: {
            left = canvas->height - y1;
            end_x = canvas->height - y0;
            top = x0;
            end_y = x1;
            break;
        }
        case 2: {
            left = canvas->width - x1;
            end_x = canvas->width - x0;
            top = canvas->height - y1;
            end_y = canvas->height - y0;
            break;
        }
        case 3: {
            left = y0;
            end_x = y1;
            top = canvas->width - x1;
            end_y = canvas->width - x0;
            break;
        }
    }
    const struct fb_info *fb = display->fb;
    volatile uint32_t *pixels = (void *)(uintptr_t)fb->framebuffer_addr;
    size_t stride = fb->framebuffer_pitch / 4;
    for (int py = top; py < end_y; py++) {
        int lx, ly;
        ui_display_position(display, left, py, &lx, &ly);
        ptrdiff_t step = display->rotation == 0 ? 1
            : display->rotation == 1 ? -canvas->width
            : display->rotation == 2 ? -1 : canvas->width;
        ptrdiff_t source = (size_t)ly * canvas->width + lx;
        for (int px = left; px < end_x; px++, source += step) {
            uint32_t value = canvas->pixels[source];
            if (fb->red_mask_shift == 0) {
                value = (value & 0xff00) | ((value >> 16) & 0xff) | ((value & 0xff) << 16);
            }
            pixels[(size_t)py * stride + px] = value;
        }
        fb_flush(pixels + (size_t)py * stride + left, (end_x - left) * 4);
    }
}
