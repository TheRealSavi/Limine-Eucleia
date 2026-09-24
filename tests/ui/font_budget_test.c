#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ui/font.h>

static size_t allocations;

void *ext_mem_alloc(uint64_t size) {
    void *ptr = calloc(1, size);
    assert(ptr != NULL);
    allocations++;
    return ptr;
}

void pmm_free(void *ptr, uint64_t size) {
    assert(ptr != NULL && size != 0 && allocations != 0);
    allocations--;
    free(ptr);
}

int main(void) {
    FILE *file = fopen("work/fonts/label.ttf", "rb");
    assert(file != NULL && fseek(file, 0, SEEK_END) == 0);
    size_t size = ftell(file);
    rewind(file);
    void *data = malloc(size);
    assert(data != NULL && fread(data, 1, size, file) == size);
    fclose(file);
    for (size_t i = 0; i < 10; i++) {
        assert(ui_font_open(data, size, 23) == NULL);
        assert(allocations == 0);
    }
    free(data);
    puts("Font memory-budget failure recovers and releases all allocations.");
    return 0;
}
