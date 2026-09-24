#include <ui/canvas.h>

static uint32_t blend(uint32_t dst, uint32_t src, unsigned alpha) {
    unsigned inv = 255 - alpha;
    uint32_t value = 0;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned channel = (((dst >> shift) & 255) * inv + ((src >> shift) & 255) * alpha + 127) / 255;
        value |= channel << shift;
    }
    return value;
}

static void pixel(struct ui_canvas *c, int x, int y, uint32_t colour, unsigned alpha) {
    if (x < 0 || y < 0 || x >= c->width || y >= c->height || alpha == 0) {
        return;
    }
    if (c->clipped && (x < c->clip_left || y < c->clip_top || x >= c->clip_right || y >= c->clip_bottom)) {
        return;
    }
    uint32_t *dst = &c->pixels[(size_t)y * c->width + x];
    *dst = alpha == 255 ? colour & 0xffffff : blend(*dst, colour, alpha);
}

void ui_rect(struct ui_canvas *c, int x, int y, int width, int height, uint32_t argb) {
    int end_x = x + width, end_y = y + height;
    if (end_x > c->width) {
        end_x = c->width;
    }
    if (end_y > c->height) {
        end_y = c->height;
    }
    for (int py = y < 0 ? 0 : y; py < end_y; py++) {
        for (int px = x < 0 ? 0 : x; px < end_x; px++) {
            pixel(c, px, py, argb, argb >> 24);
        }
    }
}

static bool rounded(int x, int y, int width, int height, int radius) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    if (radius > width / 2) { radius = width / 2; }
    if (radius > height / 2) { radius = height / 2; }
    int dx = x < radius ? radius - 1 - x : x >= width - radius ? x - (width - radius) : 0;
    int dy = y < radius ? radius - 1 - y : y >= height - radius ? y - (height - radius) : 0;
    return dx * dx + dy * dy <= radius * radius;
}

void ui_surface(struct ui_canvas *c, int x, int y, int width, int height,
    uint32_t fill, uint32_t border, int border_width, int radius) {
    int inner_radius = radius > border_width ? radius - border_width : 0;
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            if (!rounded(dx, dy, width, height, radius)) {
                continue;
            }
            bool inside = rounded(dx - border_width, dy - border_width,
                width - border_width * 2, height - border_width * 2, inner_radius);
            uint32_t colour = inside ? fill : border;
            pixel(c, x + dx, y + dy, colour, colour >> 24);
        }
    }
}

void ui_glow(struct ui_canvas *c, int x, int y, int width, int height,
    int radius, int spread, uint32_t argb) {
    if (radius > width / 2) { radius = width / 2; }
    if (radius > height / 2) { radius = height / 2; }
    for (int d = spread; d > 0; d--) {
        unsigned fade = spread - d + 1;
        unsigned alpha = (argb >> 24) * fade * fade / ((spread + 1) * (spread + 1));
        ui_surface(c, x - d, y - d, width + 2 * d, height + 2 * d,
            0, (argb & 0xffffff) | alpha << 24, 1, radius + d);
    }
}

void ui_diamond(struct ui_canvas *c, int x, int y, int size, uint32_t colour,
    int spread, uint32_t glow) {
    if (size <= 0) { return; }
    int extent = (size + 1) / 2 + spread;
    for (int dy = -extent; dy < extent; dy++) {
        for (int dx = -extent; dx < extent; dx++) {
            int ax = dx * 2 + 1, ay = dy * 2 + 1;
            if (ax < 0) { ax = -ax; }
            if (ay < 0) { ay = -ay; }
            int distance = ax + ay - size;
            if (distance < 2) {
                unsigned alpha = distance <= 0 ? 255 : 128;
                pixel(c, x + dx, y + dy, colour, alpha);
            } else if (spread > 0 && distance < 2 * spread) {
                unsigned fade = 2 * spread - distance;
                unsigned alpha = (glow >> 24) * fade * fade / (4 * spread * spread);
                pixel(c, x + dx, y + dy, glow, alpha);
            }
        }
    }
}

static unsigned interpolate(unsigned a, unsigned b, unsigned f) {
    return (a * (256 - f) + b * f + 128) >> 8;
}

static unsigned sample(const uint8_t *p, size_t stride, size_t step,
    unsigned x, unsigned y, unsigned width, unsigned height, unsigned channel) {
    unsigned ix = x >> 8, iy = y >> 8;
    unsigned nx = ix + 1 < width ? ix + 1 : ix;
    unsigned ny = iy + 1 < height ? iy + 1 : iy;
    unsigned top = interpolate(p[iy * stride + ix * step + channel],
        p[iy * stride + nx * step + channel], x & 255);
    unsigned bottom = interpolate(p[ny * stride + ix * step + channel],
        p[ny * stride + nx * step + channel], x & 255);
    return interpolate(top, bottom, y & 255);
}

void ui_image(struct ui_canvas *c, const struct ui_asset *image,
    int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    for (int dy = y < 0 ? -y : 0; dy < height && y + dy < c->height; dy++) {
        unsigned sy = (uint64_t)dy * (image->height - 1) * 256 / (height > 1 ? height - 1 : 1);
        for (int dx = x < 0 ? -x : 0; dx < width && x + dx < c->width; dx++) {
            unsigned sx = (uint64_t)dx * (image->width - 1) * 256 / (width > 1 ? width - 1 : 1);
            uint32_t colour = 0;
            for (unsigned channel = 0; channel < 4; channel++) {
                colour |= sample(image->pixels, image->width * 4, 4,
                    sx, sy, image->width, image->height, channel) << (channel * 8);
            }
            pixel(c, x + dx, y + dy, colour, colour >> 24);
        }
    }
}

static uint32_t next_codepoint(const char **text) {
    const unsigned char *p = (const void *)*text;
    uint32_t value = *p++;
    unsigned n = 0;
    uint32_t min = 0;
    if (value >= 0xc2 && value <= 0xdf) {
        n = 1; value &= 0x1f; min = 0x80;
    } else if (value >= 0xe0 && value <= 0xef) {
        n = 2; value &= 0x0f; min = 0x800;
    } else if (value >= 0xf0 && value <= 0xf4) {
        n = 3; value &= 7; min = 0x10000;
    } else if (value >= 0x80 || value < 32) {
        value = '?';
    }
    for (unsigned i = 0; i < n; i++) {
        if ((*p & 0xc0) != 0x80) {
            *text = (const char *)p;
            return '?';
        }
        value = (value << 6) | (*p++ & 0x3f);
    }
    *text = (const char *)p;
    if (value < min || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        return '?';
    }
    return value;
}

int ui_text_width(struct ui_font *font, const char *text, int tracking) {
    int64_t advance = 0;
    unsigned previous = 0;
    size_t count = 0;
    while (*text != '\0' && count++ < 4096) {
        const struct ui_glyph *g = ui_font_glyph(font, next_codepoint(&text));
        if (g == NULL) {
            break;
        }
        advance += ui_font_kerning(font, previous, g->index) + g->advance + tracking;
        previous = g->index;
    }
    if (count != 0) {
        advance -= tracking;
    }
    return (advance + 32) / 64;
}

static void glyph(struct ui_canvas *c, const struct ui_glyph *g,
    int x, int baseline, uint32_t colour, int clip_left, int clip_right) {
    x += g->left;
    int y = baseline - g->top;
    for (unsigned dy = 0; dy < g->height; dy++) {
        for (unsigned dx = 0; dx < g->width; dx++) {
            int px = x + (int)dx;
            if (px >= clip_left && px < clip_right) {
                pixel(c, px, y + dy, colour, g->pixels[dy * g->width + dx]);
            }
        }
    }
}

void ui_text(struct ui_canvas *c, struct ui_font *font, const char *text,
    int x, int baseline, int tracking, uint32_t colour, int max_width) {
    if (max_width <= 0) {
        return;
    }
    bool truncated = ui_text_width(font, text, tracking) > max_width;
    const struct ui_glyph *ellipsis = ui_font_glyph(font, 0x2026);
    if (ellipsis == NULL) {
        return;
    }
    unsigned ellipsis_index = ellipsis->index;
    int ellipsis_advance = ellipsis->advance;
    int64_t advance = 0;
    unsigned previous = 0;
    size_t count = 0;
    while (*text != '\0' && count++ < 4096) {
        const struct ui_glyph *g = ui_font_glyph(font, next_codepoint(&text));
        if (g == NULL) {
            break;
        }
        int kern = ui_font_kerning(font, previous, g->index);
        int64_t end = advance + kern + g->advance;
        int reserve = truncated ? tracking + ui_font_kerning(font, g->index, ellipsis_index)
            + ellipsis_advance : 0;
        if ((end + reserve + 32) / 64 > max_width) {
            int64_t position = advance + ui_font_kerning(font, previous, ellipsis_index);
            if (truncated && (position + ellipsis_advance + 32) / 64 <= max_width) {
                ellipsis = ui_font_glyph(font, 0x2026);
                if (ellipsis != NULL) {
                    glyph(c, ellipsis, x + (position + 32) / 64, baseline, colour, x, x + max_width);
                }
            }
            break;
        }
        glyph(c, g, x + (advance + kern + 32) / 64, baseline, colour, x, x + max_width);
        previous = g->index;
        advance = end + tracking;
    }
}
