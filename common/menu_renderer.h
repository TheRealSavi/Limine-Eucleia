#ifndef MENU_RENDERER_H__
#define MENU_RENDERER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <menu_model.h>

struct menu_style {
    const char *branding;
    const char *notice;
    uint32_t branding_colour;
    uint32_t help_colour;
    uint32_t help_colour_bright;
    bool help_hidden;
    bool editor_enabled;
    bool firmware_setup;
    bool uefi_shell;
};

struct menu_view {
    const struct menu_row *rows;
    size_t count;
    size_t selected;
    const char *comment;
    bool config_ready;
    bool countdown;
};

// The controller owns selection, expansion and actions. Renderers borrow a view
// only during draw and must not mutate it. draw prepares a frame; present flushes
// it and updates the pointer. Style strings live for the menu session.
// timeout updates and presents the countdown, or clears it when passed zero.
// hit_test consumes the active mouse space: GUI pixels or terminal cells.
// Renderers select their space and restore terminal coordinates when leaving.
// leave restores the terminal for the editor, boot messages and firmware actions;
// a later draw can resume the menu after editing is cancelled.
struct menu_renderer {
    bool (*init)(const struct menu_style *style);
    void (*draw)(const struct menu_view *view);
    void (*present)(void);
    void (*timeout)(uint64_t milliseconds);
    bool (*hit_test)(size_t x, size_t y, size_t *index);
    void (*leave)(void);
};

extern const struct menu_renderer menu_terminal_renderer;
extern const struct menu_renderer menu_gui_renderer;
const char *menu_gui_notice(void);

// The terminal is already initialised. A preferred renderer returning false
// must leave it usable and release partial resources. NULL selects the fallback.
const struct menu_renderer *menu_renderer_start(const struct menu_renderer *preferred,
    const struct menu_style *style);

#endif
