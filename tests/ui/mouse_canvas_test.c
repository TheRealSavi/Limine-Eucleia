#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <drivers/mouse.h>
#include <lib/term.h>
#include <lib/misc.h>

static struct flanterm_context terminal = {.cols = 80, .rows = 25};
static struct flanterm_context *contexts[] = {&terminal};
struct flanterm_context **terms = contexts;
size_t terms_i = 1;
int term_backend = FALLBACK;
bool efi_boot_services_exited;
static EFI_SYSTEM_TABLE system_table;
static EFI_BOOT_SERVICES boot_services;
EFI_SYSTEM_TABLE *gST = &system_table;
EFI_BOOT_SERVICES *gBS = &boot_services;
static EFI_ABSOLUTE_POINTER_MODE mode = {.AbsoluteMaxX = 32767, .AbsoluteMaxY = 32767};
static EFI_ABSOLUTE_POINTER_STATE state;
static EFI_ABSOLUTE_POINTER_PROTOCOL pointer;
static bool pending;
static size_t allocations;

char *config_get_value(const char *config, size_t index, const char *key) {
    (void)config; (void)index; (void)key;
    return NULL;
}

void *ext_mem_alloc(uint64_t size) {
    void *p = calloc(1, size);
    assert(p != NULL);
    allocations++;
    return p;
}

void *ext_mem_alloc_counted(uint64_t count, uint64_t size) {
    return ext_mem_alloc(count * size);
}

void pmm_free(void *p, uint64_t size) {
    assert(size != 0 && allocations != 0);
    allocations--;
    free(p);
}

void flanterm_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y) {
    (void)ctx; (void)x; (void)y;
}

static EFI_STATUS EFIAPI handle(EFI_HANDLE h, EFI_GUID *guid, void **interface) {
    (void)h;
    EFI_GUID absolute = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
    if (memcmp(guid, &absolute, sizeof(absolute)) != 0) {
        return EFI_UNSUPPORTED;
    }
    *interface = &pointer;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI locate(EFI_LOCATE_SEARCH_TYPE type, EFI_GUID *guid,
    void *key, UINTN *count, EFI_HANDLE **handles) {
    (void)type; (void)guid; (void)key; (void)count; (void)handles;
    return EFI_NOT_FOUND;
}

static EFI_STATUS EFIAPI reset(EFI_ABSOLUTE_POINTER_PROTOCOL *p, BOOLEAN extended) {
    (void)p; (void)extended;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI get_state(EFI_ABSOLUTE_POINTER_PROTOCOL *p, EFI_ABSOLUTE_POINTER_STATE *s) {
    (void)p;
    if (!pending) {
        return EFI_NOT_READY;
    }
    *s = state;
    pending = false;
    return EFI_SUCCESS;
}

static struct mouse_state event(uint64_t x, uint64_t y, bool down) {
    state = (EFI_ABSOLUTE_POINTER_STATE){.CurrentX = x, .CurrentY = y,
        .ActiveButtons = down ? EFI_ABSP_TouchActive : 0};
    pending = true;
    mouse_handle_efi_event(0);
    struct mouse_state result;
    mouse_get_state(&result);
    return result;
}

int main(void) {
    boot_services.HandleProtocol = handle;
    boot_services.LocateHandleBuffer = locate;
    system_table.ConsoleInHandle = (void *)1;
    pointer.Mode = &mode;
    pointer.Reset = reset;
    pointer.GetState = get_state;
    assert(mouse_init() && mouse_present());
    EFI_EVENT events[2];
    assert(mouse_get_efi_events(events, 2) == 1);
    struct mouse_state s = event(0, 0, false);
    assert(s.x == 0 && s.y == 0);
    mouse_set_canvas(1920, 1080);
    s = event(32767, 32767, false);
    assert(s.x == 1919 && s.y == 1079 && s.moved);
    s = event(16383, 16383, false);
    assert(s.x == 959 && s.y == 539);
    s = event(10000, 10000, true);
    size_t px = s.x, py = s.y;
    s = event(10020, 10020, false);
    assert(s.click && s.press_x == px && s.press_y == py && s.click_y != py);
    event(10000, 10000, true);
    mouse_flush();
    s = event(10000, 10000, false);
    assert(!s.click);
    mouse_set_canvas(0, 0);
    s = event(32767, 32767, false);
    assert(s.x == 79 && s.y == 24);
    event(10000, 10000, true);
    s = event(10000, 20000, false);
    assert(!s.click);
    event(10000, 10000, true);
    s = event(10000, 10000, false);
    assert(s.click);
    mouse_set_canvas(0, 1080);
    s = event(32767, 32767, false);
    assert(s.x == 79 && s.y == 24);
    mouse_deinit();
    assert(!mouse_present() && allocations == 0);
    const char message[] = "UEFI pointer conversion, click origin and input-flush checks passed.\n";
    assert(write(1, message, sizeof(message) - 1) == sizeof(message) - 1);
    return 0;
}
