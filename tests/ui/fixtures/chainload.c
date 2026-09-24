#include <efi.h>

static void debug(const char *s) {
    while (*s != 0) {
        __asm__ volatile ("outb %0, %1" : : "a"(*s++), "Nd"((UINT16)0xe9));
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    EFI_GUID guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    if (st->BootServices->HandleProtocol(image, &guid, (void **)&loaded) != EFI_SUCCESS) {
        return EFI_LOAD_ERROR;
    }
    const CHAR16 *options = loaded->LoadOptions;
    const char expected[] = "eucleia-chainload";
    if (options == NULL || loaded->LoadOptionsSize < sizeof(expected) * sizeof(CHAR16)) {
        return EFI_INVALID_PARAMETER;
    }
    for (UINTN i = 0; i < sizeof(expected); i++) {
        if (options[i] != (CHAR16)expected[i]) {
            return EFI_INVALID_PARAMETER;
        }
    }
    st->ConOut->OutputString(st->ConOut, L"Eucleia EFI chainload fixture passed.\r\n");
    debug("EUCLEIA_EFI_OPTIONS_OK\n");
    for (;;) {
        st->BootServices->Stall(100000);
    }
}
