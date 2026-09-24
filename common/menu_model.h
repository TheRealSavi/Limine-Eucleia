#ifndef MENU_MODEL_H__
#define MENU_MODEL_H__

#include <stdbool.h>
#include <stddef.h>

struct menu_entry;

struct menu_row {
    const char *name;
    size_t parent;
    size_t depth;
    size_t duplicate;
    bool directory;
    bool expanded;
    bool has_next_sibling;
};

struct menu_model {
    struct menu_row *rows;
    struct menu_entry **entries;
    size_t count;
};

size_t menu_model_count(struct menu_entry *tree, bool (*skip)(struct menu_entry *));
// Strings are borrowed from the tree; rows and entries remain valid until update.
// Updating with an empty tree releases the projection. Initialise model to zero.
void menu_model_update(struct menu_model *model, struct menu_entry *tree,
    bool (*skip)(struct menu_entry *));
struct menu_entry *menu_model_entry(const struct menu_model *model, size_t index);
bool menu_row_matches(const struct menu_row *rows, size_t index, const char *path);

#endif
