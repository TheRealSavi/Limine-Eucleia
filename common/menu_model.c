#include <stdint.h>
#include <menu_model.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <mm/pmm.h>

size_t menu_model_count(struct menu_entry *tree, bool (*skip)(struct menu_entry *)) {
    size_t count = 0;
    for (struct menu_entry *entry = tree; entry != NULL; entry = entry->next) {
        if (skip(entry)) {
            continue;
        }
        count++;
        if (entry->sub != NULL && entry->expanded) {
            count += menu_model_count(entry->sub, skip);
        }
    }
    return count;
}

static void project_tree(struct menu_model *model, struct menu_entry *tree,
    bool (*skip)(struct menu_entry *), size_t parent, size_t depth, size_t *index) {
    for (struct menu_entry *entry = tree; entry != NULL; entry = entry->next) {
        if (skip(entry)) {
            continue;
        }
        size_t current = (*index)++;
        model->entries[current] = entry;
        model->rows[current] = (struct menu_row){
            .name = entry->name,
            .parent = parent,
            .depth = depth,
            .directory = entry->sub != NULL,
            .expanded = entry->expanded,
            .has_next_sibling = entry->next != NULL
        };
        for (struct menu_entry *prior = tree; prior != entry; prior = prior->next) {
            if (!skip(prior) && strcmp(prior->name, entry->name) == 0) {
                model->rows[current].duplicate++;
            }
        }
        if (entry->sub != NULL && entry->expanded) {
            project_tree(model, entry->sub, skip, current, depth + 1, index);
        }
    }
}

void menu_model_update(struct menu_model *model, struct menu_entry *tree,
    bool (*skip)(struct menu_entry *)) {
    size_t count = menu_model_count(tree, skip);
    if (count != model->count) {
        if (model->count != 0) {
            pmm_free(model->rows, model->count * sizeof(*model->rows));
            pmm_free(model->entries, model->count * sizeof(*model->entries));
        }
        model->rows = count == 0 ? NULL : ext_mem_alloc_counted(count, sizeof(*model->rows));
        model->entries = count == 0 ? NULL : ext_mem_alloc_counted(count, sizeof(*model->entries));
        model->count = count;
    }
    size_t index = 0;
    project_tree(model, tree, skip, SIZE_MAX, 0, &index);
}

struct menu_entry *menu_model_entry(const struct menu_model *model, size_t index) {
    return index < model->count ? model->entries[index] : NULL;
}

bool menu_row_matches(const struct menu_row *rows, size_t index, const char *path) {
    size_t chain[64], count = 0;
    for (size_t at = index; at != SIZE_MAX; at = rows[at].parent) {
        if (count == 64 || (rows[at].parent != SIZE_MAX && rows[at].parent >= at)) {
            return false;
        }
        chain[count++] = at;
    }
    while (count != 0) {
        const struct menu_row *row = &rows[chain[--count]];
        const char *name = row->name;
        while (*name != 0) {
            if (*name == '/' || *name == '#' || *name == '\\') {
                if (*path++ != '\\') { return false; }
            }
            if (*path++ != *name++) { return false; }
        }
        size_t duplicate = 0;
        if (*path == '#') {
            path++;
            if (*path < '0' || *path > '9') { return false; }
            while (*path >= '0' && *path <= '9') {
                if (duplicate > (SIZE_MAX - 9) / 10) { return false; }
                duplicate = duplicate * 10 + *path++ - '0';
            }
        }
        if (duplicate != row->duplicate || (count != 0 && *path++ != '/')) {
            return false;
        }
    }
    return *path == 0;
}
