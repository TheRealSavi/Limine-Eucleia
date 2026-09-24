#include <ui/assets.h>
#include <lib/misc.h>
#include <lib/stb_image.h>
#include <mm/pmm.h>
#include <fs/file.h>

void *ui_read_file(const char *path, size_t limit, size_t *size) {
    struct file_handle *file = fopen(boot_volume, path);
    if (file == NULL) {
        return NULL;
    }
    if (file->size == 0 || file->size > limit) {
        fclose(file);
        return NULL;
    }
    *size = file->size;
    void *data = ext_mem_alloc(*size + 1);
    bool ok = fread(file, data, 0, *size) == *size;
    fclose(file);
    if (!ok) {
        pmm_free(data, *size + 1);
        return NULL;
    }
    ((char *)data)[*size] = 0;
    return data;
}

bool ui_png_read(struct ui_asset *asset, const char *path, unsigned max_dimension,
    size_t *remaining) {
    size_t size;
    uint8_t *data = ui_read_file(path, 4 * 1024 * 1024, &size);
    if (data == NULL) {
        return false;
    }
    const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    int width, height, channels;
    bool valid = size >= sizeof(signature) && memcmp(data, signature, sizeof(signature)) == 0
        && stbi_info_from_memory(data, size, &width, &height, &channels)
        && width > 0 && height > 0 && (unsigned)width <= max_dimension && (unsigned)height <= max_dimension
        && (uint64_t)width * height * 4 <= *remaining;
    uint8_t *pixels = valid ? stbi_load_from_memory(data, size, &width, &height, &channels, 4) : NULL;
    pmm_free(data, size + 1);
    if (pixels == NULL) {
        return false;
    }
    if (width <= 0 || height <= 0 || (unsigned)width > max_dimension || (unsigned)height > max_dimension
     || (uint64_t)width * height * 4 > *remaining) {
        stbi_image_free(pixels);
        return false;
    }
    size_t bytes = (size_t)width * height * 4;
    for (size_t i = 0; i < bytes; i += 4) {
        uint8_t red = pixels[i];
        pixels[i] = pixels[i + 2];
        pixels[i + 2] = red;
    }
    *asset = (struct ui_asset){1, width, height, pixels, bytes};
    *remaining -= bytes;
    return true;
}

void ui_png_free(struct ui_asset *asset) {
    if (asset->pixels != NULL) {
        stbi_image_free((void *)asset->pixels);
        asset->pixels = NULL;
    }
}
