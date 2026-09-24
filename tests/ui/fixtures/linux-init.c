#include <stddef.h>

__attribute__((noreturn)) void _start(void) {
    static const char message[] = "EUCLEIA_LINUX_INIT_OK\n";
    long result;
    __asm__ volatile ("syscall" : "=a"(result)
        : "a"(1L), "D"(1L), "S"(message), "d"(sizeof(message) - 1)
        : "rcx", "r11", "memory");
    for (;;) {
        __asm__ volatile ("syscall" : "=a"(result) : "a"(34L) : "rcx", "r11", "memory");
    }
}
