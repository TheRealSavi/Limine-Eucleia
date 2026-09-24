#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <menu_renderer.h>
#include <lib/libc.h>
#include <lib/print.h>
#include <lib/term.h>
#include <lib/misc.h>
#include <drivers/mouse.h>

static struct menu_style style;
static char interface_help_colour[24];
static char interface_help_colour_bright[24];
static char menu_branding_colour[24];
static size_t tree_offset;
static size_t tree_row_start;
static size_t tree_window;
static size_t tree_count;

static char *append_uint_dec(char *p, uint64_t val) {
    char buf[20];
    size_t i = 0;

    do {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    } while (val != 0);

    while (i != 0) {
        *p++ = buf[--i];
    }
    *p = '\0';
    return p;
}

static size_t format_timeout_ms(char *buf, uint64_t milliseconds) {
    char *p = append_uint_dec(buf, milliseconds / 1000);
    uint64_t subsecond = milliseconds % 1000;

    if (subsecond != 0) {
        char *last;

        *p++ = '.';
        *p++ = '0' + subsecond / 100;
        *p++ = '0' + (subsecond / 10) % 10;
        *p++ = '0' + subsecond % 10;

        last = p - 1;
        while (*last == '0') {
            last--;
        }
        p = last + 1;
    }

    *p = '\0';
    return p - buf;
}

static size_t help_action_len(const char *label) {
    return 2 + strlen(label);
}

static void add_help_action_len(size_t *len, size_t *count, const char *label) {
    *len += help_action_len(label);
    if ((*count)++ != 0) {
        *len += 4;
    }
}

static void print_help_action(const char *key, const char *label, bool *need_separator) {
    if (*need_separator) {
        print("    ");
    }
    *need_separator = true;
    print("%s%s\e[0m %s", interface_help_colour, key, label);
}

static void print_secondary_help(size_t row, bool firmware_setup, bool uefi_shell, bool blank_entry) {
    const char *firmware_setup_label = "Firmware Setup";
    const char *uefi_shell_label = "UEFI Shell";
    const char *blank_entry_label = "Blank Entry";

    size_t len = 0;
    size_t count = 0;

    if (firmware_setup) {
        add_help_action_len(&len, &count, firmware_setup_label);
    }
    if (uefi_shell) {
        add_help_action_len(&len, &count, uefi_shell_label);
    }
    if (blank_entry) {
        add_help_action_len(&len, &count, blank_entry_label);
    }

    if (len > terms[0]->cols) {
        firmware_setup_label = "Setup";
        uefi_shell_label = "Shell";
        blank_entry_label = "Blank";

        len = 0;
        count = 0;
        if (firmware_setup) {
            add_help_action_len(&len, &count, firmware_setup_label);
        }
        if (uefi_shell) {
            add_help_action_len(&len, &count, uefi_shell_label);
        }
        if (blank_entry) {
            add_help_action_len(&len, &count, blank_entry_label);
        }
    }

    set_cursor_pos_helper((terms[0]->cols > len) ? (terms[0]->cols - len) / 2 : 0, row);

    bool need_separator = false;
    if (firmware_setup) {
        print_help_action("S", firmware_setup_label, &need_separator);
    }
    if (uefi_shell) {
        print_help_action("U", uefi_shell_label, &need_separator);
    }
    if (blank_entry) {
        print_help_action("B", blank_entry_label, &need_separator);
    }
}

static void print_entry_comment(const char *comment, size_t row) {
    if (comment == NULL) {
        return;
    }

    size_t comment_len = strlen(comment);
    size_t max_len = terms[0]->cols - 2;
    FOR_TERM(TERM->scroll_enabled = false);
    if (comment_len <= max_len) {
        set_cursor_pos_helper((terms[0]->cols - comment_len) / 2, row);
        print("\e[36m%s\e[0m", comment);
    } else {
        size_t keep = max_len > 3 ? max_len - 3 : 0;
        set_cursor_pos_helper(1, row);
        print("\e[36m%S...\e[0m", comment, keep);
    }
    FOR_TERM(TERM->scroll_enabled = true);
}

static bool terminal_init(const struct menu_style *options) {
    style = *options;
    term_format_fg_rgb_escape(interface_help_colour, style.help_colour);
    term_format_fg_rgb_escape(interface_help_colour_bright, style.help_colour_bright);
    term_format_fg_rgb_escape(menu_branding_colour, style.branding_colour);
    tree_offset = 0;
    tree_count = 0;
    return true;
}

static void print_rows(const struct menu_view *view, size_t prefix_len) {
    size_t end = tree_offset + tree_window;
    if (end > view->count) {
        end = view->count;
    }
    for (size_t index = tree_offset; index < end; index++) {
        const struct menu_row *row = &view->rows[index];
        for (size_t i = 0; i < prefix_len; i++) {
            print(" ");
        }
        for (size_t i = row->depth; i > 1; i--) {
            const struct menu_row *parent = row;
            for (size_t j = 1; j < i; j++) {
                parent = &view->rows[parent->parent];
            }
            print(parent->has_next_sibling ? (SERIAL_CONSOLE ? " |" : " \u2502") : "  ");
        }
        if (row->depth != 0) {
            if (row->has_next_sibling) {
                print(SERIAL_CONSOLE ? " |" : " \u251c");
            } else {
                print(SERIAL_CONSOLE ? " `" : " \u2514");
            }
        }
        if (row->directory) {
            print(row->expanded ? "[-]" : "[+]");
        } else if (row->depth != 0) {
            print(SERIAL_CONSOLE ? "-->" : "\u2500\u2500\u25ba");
        } else {
            print("   ");
        }
        if (index == view->selected) {
            print("\e[7m");
        }
        size_t used = prefix_len + row->depth * 2 + 5;
        size_t max_name = terms[0]->cols > used ? terms[0]->cols - used : 0;
        if (strlen(row->name) > max_name && max_name > 3) {
            print(" %S...\e[27m\n", row->name, max_name - 3);
        } else {
            print(" %s \e[27m\n", row->name);
        }
    }
}

static void terminal_draw(const struct menu_view *view) {
    mouse_erase_pointer();
    bool secondary_help = style.editor_enabled || style.firmware_setup || style.uefi_shell;
    size_t header_offset = style.branding[0] != '\0' ? 2 : 0;
    if (secondary_help) {
        header_offset += 2;
    }
    tree_count = view->count;
    tree_window = terms[0]->rows - 8 - header_offset;
    if (view->selected >= tree_offset + tree_window) {
        tree_offset = view->selected - tree_window + 1;
    }
    if (view->selected < tree_offset) {
        tree_offset = view->selected;
    }

    FOR_TERM(TERM->autoflush = false);
    FOR_TERM(TERM->cursor_enabled = false);
    print("\e[2J\e[H\n");
    if (style.branding[0] != '\0') {
        size_t x, y;
        terms[0]->get_cursor_pos(terms[0], &x, &y);
        size_t branding_len = strlen(style.branding);
        size_t max_len = terms[0]->cols - 2;
        if (branding_len <= max_len) {
            set_cursor_pos_helper((terms[0]->cols - branding_len) / 2, y);
            print("%s%s\e[0m", menu_branding_colour, style.branding);
        } else {
            size_t keep = max_len > 3 ? max_len - 3 : 0;
            set_cursor_pos_helper(1, y);
            print("%s%S...\e[0m", menu_branding_colour, style.branding, keep);
        }
        print("\n\n\n\n");
    }
    if (view->count == 0) {
        const char *msg = view->config_ready
            ? "[config file contains no valid entries]" : "[config file not found]";
        set_cursor_pos_helper((terms[0]->cols - strlen(msg)) / 2, (terms[0]->rows - 1) / 2);
        print("%s\n", msg);
    } else {
        size_t max_len = 0;
        for (size_t i = 0; i < view->count; i++) {
            size_t len = view->rows[i].depth * 2 + 5 + strlen(view->rows[i].name);
            if (len > max_len) {
                max_len = len;
            }
        }
        size_t prefix_len = terms[0]->cols > max_len + 3 ? (terms[0]->cols - max_len - 3) / 2 : 1;
        size_t height = view->count < tree_window ? view->count : tree_window;
        tree_row_start = (terms[0]->rows - height) / 2;
        if (tree_row_start < 4 + header_offset) {
            tree_row_start = 4 + header_offset;
        }
        set_cursor_pos_helper(0, tree_row_start);
        print_rows(view, prefix_len);
    }

    size_t x, y;
    terms[0]->get_cursor_pos(terms[0], &x, &y);
    if (view->count != 0) {
        if (tree_offset > 0) {
            set_cursor_pos_helper((terms[0]->cols - 3) / 2, 3 + header_offset);
            print(SERIAL_CONSOLE ? "^^^" : "\u2191\u2191\u2191");
        }
        if (tree_offset + tree_window < view->count) {
            set_cursor_pos_helper((terms[0]->cols - 3) / 2, terms[0]->rows - 4);
            print(SERIAL_CONSOLE ? "vvv" : "\u2193\u2193\u2193");
        }
    }
    if (!style.help_hidden) {
        if (view->count != 0) {
            const struct menu_row *selected = &view->rows[view->selected];
            size_t primary_row = 1 + header_offset - (secondary_help ? 2 : 0);
            if (!selected->directory) {
                if (style.editor_enabled) {
                    set_cursor_pos_helper((terms[0]->cols - 37) / 2, primary_row);
                    print("%sARROWS\e[0m Select    %sENTER\e[0m Boot    %sE\e[0m Edit",
                          interface_help_colour, interface_help_colour, interface_help_colour);
                } else {
                    set_cursor_pos_helper((terms[0]->cols - 27) / 2, primary_row);
                    print("%sARROWS\e[0m Select    %sENTER\e[0m Boot",
                          interface_help_colour, interface_help_colour);
                }
            } else {
                const char *action = selected->expanded ? "Collapse" : "Expand";
                size_t len = 23 + strlen(action);
                set_cursor_pos_helper((terms[0]->cols - len) / 2, primary_row);
                print("%sARROWS\e[0m Select    %sENTER\e[0m %s",
                      interface_help_colour, interface_help_colour, action);
            }
        }
        if (secondary_help) {
            print_secondary_help(1 + header_offset, style.firmware_setup, style.uefi_shell, style.editor_enabled);
        }
    }
    set_cursor_pos_helper(x, y);
    if (view->count != 0) {
        if (view->countdown) {
            print("\n\n");
        }
        print_entry_comment(view->comment, terms[0]->rows - (view->countdown ? 3 : 2));
    }
    if (style.notice != NULL && style.notice[0] != '\0') {
        print_entry_comment(style.notice, 0);
    }
}

static void terminal_present(void) {
    FOR_TERM(TERM->double_buffer_flush(TERM));
    mouse_render_pointer();
}

static void terminal_timeout(uint64_t milliseconds) {
    mouse_erase_pointer();
    if (milliseconds == 0) {
        print("\e[2K");
    } else {
        char timeout_buf[24];
        size_t msg_len = 28 + format_timeout_ms(timeout_buf, milliseconds);
        set_cursor_pos_helper((terms[0]->cols - msg_len) / 2, terms[0]->rows - 2);
        FOR_TERM(TERM->scroll_enabled = false);
        print("\e[2K%sBooting automatically in %s%s%s...\e[0m",
              interface_help_colour, interface_help_colour_bright, timeout_buf, interface_help_colour);
        FOR_TERM(TERM->scroll_enabled = true);
    }
    FOR_TERM(TERM->double_buffer_flush(TERM));
    if (milliseconds != 0) {
        mouse_render_pointer();
    }
}

static bool terminal_hit_test(size_t x, size_t y, size_t *index) {
    (void)x;
    if (y < tree_row_start || y >= tree_row_start + tree_window) {
        return false;
    }
    size_t target = tree_offset + y - tree_row_start;
    if (target >= tree_count) {
        return false;
    }
    *index = target;
    return true;
}

static void terminal_leave(void) {
    mouse_erase_pointer();
    FOR_TERM(TERM->autoflush = true);
    FOR_TERM(TERM->cursor_enabled = true);
    FOR_TERM(TERM->scroll_enabled = true);
    tree_count = 0;
}

const struct menu_renderer menu_terminal_renderer = {
    .init = terminal_init,
    .draw = terminal_draw,
    .present = terminal_present,
    .timeout = terminal_timeout,
    .hit_test = terminal_hit_test,
    .leave = terminal_leave
};
