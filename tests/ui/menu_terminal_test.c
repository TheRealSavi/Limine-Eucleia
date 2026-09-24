#include <assert.h>
#include <stdio.h>
#include <menu_renderer.h>
#include <lib/term.h>

static struct flanterm_context terminal;
static struct flanterm_context *contexts[] = {&terminal};
struct flanterm_context **terms = contexts;
size_t terms_i = 1;
bool serial = false;
static size_t flushes, pointers, erasures;

void print(const char *fmt, ...) {
    (void)fmt;
}

void term_format_fg_rgb_escape(char *buf, uint32_t rgb) {
    (void)rgb;
    buf[0] = '\0';
}

void mouse_erase_pointer(void) {
    erasures++;
}

void mouse_render_pointer(void) {
    pointers++;
}

static void flush(struct flanterm_context *ctx) {
    assert(ctx == &terminal);
    flushes++;
}

static void cursor(struct flanterm_context *ctx, size_t *x, size_t *y) {
    assert(ctx == &terminal);
    *x = *y = 0;
}

int main(void) {
    terminal.cols = 80;
    terminal.rows = 25;
    terminal.double_buffer_flush = flush;
    terminal.get_cursor_pos = cursor;
    const struct menu_renderer *renderer = &menu_terminal_renderer;
    struct menu_style style = {.branding = "test", .editor_enabled = true};
    assert(renderer->init(&style));
    struct menu_row rows[40];
    for (size_t i = 0; i < 40; i++) {
        rows[i] = (struct menu_row){.name = "Entry", .parent = SIZE_MAX};
    }
    struct menu_view view = {.rows = rows, .count = 3};
    renderer->draw(&view);
    size_t index;
    assert(!renderer->hit_test(40, 10, &index));
    assert(renderer->hit_test(0, 11, &index) && index == 0);
    assert(renderer->hit_test(79, 13, &index) && index == 2);
    assert(!renderer->hit_test(40, 14, &index));
    assert(!renderer->hit_test(40, SIZE_MAX, &index));
    assert(!terminal.autoflush && !terminal.cursor_enabled);
    renderer->present();
    assert(flushes == 1 && pointers == 1 && erasures == 1);

    view.count = 40;
    view.selected = 39;
    renderer->draw(&view);
    assert(renderer->hit_test(40, 8, &index) && index == 27);
    assert(renderer->hit_test(40, 20, &index) && index == 39);
    assert(!renderer->hit_test(40, 21, &index));
    view.selected = 0;
    renderer->draw(&view);
    assert(renderer->hit_test(40, 8, &index) && index == 0);

    renderer->leave();
    assert(terminal.autoflush && terminal.cursor_enabled && terminal.scroll_enabled);
    assert(!renderer->hit_test(40, 8, &index));
    view.count = 3;
    renderer->draw(&view);
    assert(renderer->hit_test(40, 11, &index) && index == 0);
    view.count = 0;
    renderer->draw(&view);
    assert(!renderer->hit_test(40, 11, &index));

    puts("Terminal hit testing and presentation lifecycle checks passed.");
    return 0;
}
