#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lib/config.h>
#include <menu_model.h>
#include <menu_renderer.h>

static size_t allocations;

void *ext_mem_alloc_counted(uint64_t count, uint64_t size) {
    assert(count != 0 && size <= SIZE_MAX / count);
    void *ptr = calloc(count, size);
    assert(ptr != NULL);
    allocations++;
    return ptr;
}

void pmm_free(void *ptr, uint64_t size) {
    assert(ptr != NULL && size != 0 && allocations != 0);
    allocations--;
    free(ptr);
}

static bool skip(struct menu_entry *entry) {
    return entry->name[0] == '!';
}

static int fallback_calls;
static int preferred_calls;
static bool preferred_ready;

static bool fallback_init(const struct menu_style *style) {
    assert(strcmp(style->branding, "test") == 0);
    fallback_calls++;
    return true;
}

static bool preferred_init(const struct menu_style *style) {
    assert(style->editor_enabled);
    preferred_calls++;
    return preferred_ready;
}

const struct menu_renderer menu_terminal_renderer = {.init = fallback_init};

int main(void) {
    struct menu_entry hidden = {.name = "!hidden"};
    struct menu_entry leaf = {.name = "leaf", .next = &hidden};
    struct menu_entry nested = {.name = "nested", .sub = &leaf, .expanded = true};
    struct menu_entry child = {.name = "child", .next = &nested};
    struct menu_entry last = {.name = "last"};
    struct menu_entry group = {.name = "group", .sub = &child, .next = &last};
    struct menu_entry first = {.name = "first", .next = &group};
    child.parent = nested.parent = &group;
    leaf.parent = hidden.parent = &nested;
    struct menu_model model = {0};

    menu_model_update(&model, &first, skip);
    assert(model.count == 3 && menu_model_count(&first, skip) == 3);
    assert(menu_model_entry(&model, 1) == &group);
    assert(menu_model_entry(&model, 2) == &last);
    assert(menu_model_entry(&model, 3) == NULL);
    assert(menu_model_entry(&model, SIZE_MAX) == NULL);
    assert(model.rows[1].directory && !model.rows[1].expanded);
    assert(model.rows[0].parent == SIZE_MAX);

    group.expanded = true;
    menu_model_update(&model, &first, skip);
    assert(model.count == 6 && menu_model_count(&first, skip) == 6);
    assert(menu_model_entry(&model, 4) == &leaf);
    assert(menu_model_entry(&model, 5) == &last);
    assert(model.rows[4].depth == 2 && model.rows[4].parent == 3);
    assert(model.rows[3].parent == 1 && model.rows[3].directory);
    assert(model.rows[4].has_next_sibling);
    assert(strcmp(model.rows[4].name, "leaf") == 0);
    assert(menu_row_matches(model.rows, 4, "group/nested/leaf"));
    assert(!menu_row_matches(model.rows, 4, "group/leaf"));
    assert(!menu_row_matches(model.rows, 4, "group/nested/"));
    assert(!menu_row_matches(model.rows, 4, ""));

    // A changed tree with the same row count must refresh borrowed names/pointers.
    struct menu_entry replacement = {.name = "replacement", .next = &hidden};
    nested.sub = &replacement;
    menu_model_update(&model, &first, skip);
    assert(menu_model_entry(&model, 4) == &replacement);
    assert(strcmp(model.rows[4].name, "replacement") == 0);
    assert(group.expanded && nested.expanded);

    group.expanded = false;
    menu_model_update(&model, &first, skip);
    assert(model.count == 3 && menu_model_entry(&model, 2) == &last);
    menu_model_update(&model, NULL, skip);
    assert(model.count == 0 && model.rows == NULL && model.entries == NULL);
    assert(allocations == 0);
    menu_model_update(&model, &hidden, skip);
    assert(model.count == 0 && allocations == 0);

    struct menu_entry duplicate = {.name = "same/#\\"};
    struct menu_entry original = {.name = "same/#\\", .next = &duplicate};
    menu_model_update(&model, &original, skip);
    assert(menu_row_matches(model.rows, 0, "same\\/\\#\\\\"));
    assert(menu_row_matches(model.rows, 1, "same\\/\\#\\\\#1"));
    assert(!menu_row_matches(model.rows, 1, "same\\/\\#\\\\"));
    assert(!menu_row_matches(model.rows, 0, "same\\/\\#\\\\#999999999999999999999999"));
    menu_model_update(&model, NULL, skip);
    assert(allocations == 0);

    struct menu_style style = {.branding = "test", .editor_enabled = true};
    const struct menu_renderer preferred = {.init = preferred_init};
    assert(menu_renderer_start(NULL, &style) == &menu_terminal_renderer);
    assert(fallback_calls == 1 && preferred_calls == 0);
    assert(menu_renderer_start(&preferred, &style) == &menu_terminal_renderer);
    assert(fallback_calls == 2 && preferred_calls == 1);
    preferred_ready = true;
    assert(menu_renderer_start(&preferred, &style) == &preferred);
    assert(fallback_calls == 2 && preferred_calls == 2);

    puts("Menu projection and renderer fallback checks passed.");
    return 0;
}
