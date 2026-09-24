#include <ui/freetype/runtime.h>

static ui_ft_jump_buffer recovery;
static volatile unsigned calls;

static __attribute__((noinline)) void nested(unsigned depth) {
    volatile uintptr_t stack[32];
    for (unsigned i = 0; i < 32; i++) {
        stack[i] = i + depth;
    }
    calls++;
    if (depth != 0) {
        nested(depth - 1);
    } else {
        ft_longjmp(recovery, 1);
    }
    calls = stack[0];
}

static __attribute__((noreturn)) void finish(long status) {
#if defined (__aarch64__)
    register long result __asm__("x0") = status;
    register long number __asm__("x8") = 93;
    __asm__ volatile ("svc 0" : : "r"(result), "r"(number) : "memory");
#elif defined (__riscv)
    register long result __asm__("a0") = status;
    register long number __asm__("a7") = 93;
    __asm__ volatile ("ecall" : : "r"(result), "r"(number) : "memory");
#elif defined (__loongarch64)
    register long result __asm__("a0") = status;
    register long number __asm__("a7") = 93;
    __asm__ volatile ("syscall 0" : : "r"(result), "r"(number) : "memory");
#endif
    __builtin_unreachable();
}

__attribute__((noreturn)) void _start(void) {
    volatile uintptr_t guard[64];
    for (unsigned i = 0; i < 64; i++) {
        guard[i] = 0x12340000 + i;
    }
    for (volatile unsigned round = 0; round < 128; round++) {
        calls = 0;
        int result = ft_setjmp(recovery);
        if (result == 0) {
            nested(8);
            finish(1);
        }
        if (result != 1 || calls != 9) {
            finish(2);
        }
        for (unsigned i = 0; i < 64; i++) {
            if (guard[i] != 0x12340000 + i) {
                finish(3);
            }
        }
    }
    finish(0);
}
