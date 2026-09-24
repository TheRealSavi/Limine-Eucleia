#ifndef UI__FREETYPE_STDLIB_H__
#define UI__FREETYPE_STDLIB_H__

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <lib/libc.h>

#define ft_ptrdiff_t ptrdiff_t
#define FT_CHAR_BIT CHAR_BIT
#define FT_USHORT_MAX USHRT_MAX
#define FT_INT_MAX INT_MAX
#define FT_INT_MIN INT_MIN
#define FT_UINT_MAX UINT_MAX
#define FT_LONG_MIN LONG_MIN
#define FT_LONG_MAX LONG_MAX
#define FT_ULONG_MAX ULONG_MAX
#define FT_LLONG_MAX LLONG_MAX
#define FT_LLONG_MIN LLONG_MIN
#define FT_ULLONG_MAX ULLONG_MAX

#define ft_memchr memchr
#define ft_memcmp memcmp
#define ft_memcpy memcpy
#define ft_memmove memmove
#define ft_memset memset
#define ft_strcmp strcmp
#define ft_strcpy strcpy
#define ft_strlen strlen
#define ft_strncmp strncmp
#define ft_strncpy strncpy
#define ft_strrchr strrchr
#define ft_strstr ui_ft_strstr
#define ft_qsort ui_ft_qsort

char *ui_ft_strstr(const char *text, const char *needle);
void ui_ft_qsort(void *base, size_t count, size_t size,
    int (*compare)(const void *, const void *));

#if defined (__i386__) || defined (__x86_64__)
typedef void *ui_ft_jump_buffer[5];
#define ft_jmp_buf ui_ft_jump_buffer
#define ft_setjmp(buffer) __builtin_setjmp((void **)(buffer))
#define ft_longjmp(buffer, value) __builtin_longjmp((void **)(buffer), 1)
#else
typedef uintptr_t ui_ft_jump_buffer[14];
int ui_ft_save(volatile uintptr_t *buffer) __attribute__((returns_twice));
void ui_ft_restore(volatile uintptr_t *buffer) __attribute__((noreturn));
#define ft_jmp_buf ui_ft_jump_buffer
#define ft_setjmp(buffer) ui_ft_save(buffer)
#define ft_longjmp(buffer, value) ui_ft_restore(buffer)
#endif

#endif
