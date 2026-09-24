#ifndef UI__SUPPORT_H__
#define UI__SUPPORT_H__

#include <ui/canvas.h>
#include <ui/config.h>

void ui_draw_hints(struct ui_canvas *canvas, struct ui_font *font,
    const struct ui_settings *settings, const char *const labels[UI_HINT_COUNT]);
void ui_draw_details(struct ui_canvas *canvas, struct ui_font *font,
    const struct ui_settings *settings, const char *text, const struct ui_rect *selection);

#endif
