#include <menu_renderer.h>

const struct menu_renderer *menu_renderer_start(const struct menu_renderer *preferred,
    const struct menu_style *style) {
    if (preferred != NULL && preferred->init(style)) {
        return preferred;
    }
    menu_terminal_renderer.init(style);
    return &menu_terminal_renderer;
}
