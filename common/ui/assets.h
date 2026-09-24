#ifndef UI__ASSETS_H__
#define UI__ASSETS_H__

#include <ui/theme.h>

void *ui_read_file(const char *path, size_t limit, size_t *size);
bool ui_png_read(struct ui_asset *asset, const char *path, unsigned max_dimension,
    size_t *remaining);
void ui_png_free(struct ui_asset *asset);

#endif
