// These targets use integer-only ABIs throughout the bootloader.
#if defined (__aarch64__)
.text
.p2align 2
.global ui_ft_save
.type ui_ft_save, %function
ui_ft_save:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x1, sp
    str x1, [x0, #96]
    mov x0, #0
    ret
.size ui_ft_save, . - ui_ft_save

.global ui_ft_restore
.type ui_ft_restore, %function
ui_ft_restore:
    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldp x29, x30, [x0, #80]
    ldr x1, [x0, #96]
    mov sp, x1
    mov x0, #1
    ret
.size ui_ft_restore, . - ui_ft_restore

#elif defined (__riscv)
.text
.p2align 2
.global ui_ft_save
.type ui_ft_save, @function
ui_ft_save:
    sd s0, 0(a0)
    sd s1, 8(a0)
    sd s2, 16(a0)
    sd s3, 24(a0)
    sd s4, 32(a0)
    sd s5, 40(a0)
    sd s6, 48(a0)
    sd s7, 56(a0)
    sd s8, 64(a0)
    sd s9, 72(a0)
    sd s10, 80(a0)
    sd s11, 88(a0)
    sd sp, 96(a0)
    sd ra, 104(a0)
    li a0, 0
    ret
.size ui_ft_save, . - ui_ft_save

.global ui_ft_restore
.type ui_ft_restore, @function
ui_ft_restore:
    ld s0, 0(a0)
    ld s1, 8(a0)
    ld s2, 16(a0)
    ld s3, 24(a0)
    ld s4, 32(a0)
    ld s5, 40(a0)
    ld s6, 48(a0)
    ld s7, 56(a0)
    ld s8, 64(a0)
    ld s9, 72(a0)
    ld s10, 80(a0)
    ld s11, 88(a0)
    ld sp, 96(a0)
    ld ra, 104(a0)
    li a0, 1
    ret
.size ui_ft_restore, . - ui_ft_restore

#elif defined (__loongarch64)
.text
.p2align 2
.global ui_ft_save
.type ui_ft_save, @function
ui_ft_save:
    st.d $s0, $a0, 0
    st.d $s1, $a0, 8
    st.d $s2, $a0, 16
    st.d $s3, $a0, 24
    st.d $s4, $a0, 32
    st.d $s5, $a0, 40
    st.d $s6, $a0, 48
    st.d $s7, $a0, 56
    st.d $s8, $a0, 64
    st.d $fp, $a0, 72
    st.d $sp, $a0, 80
    st.d $ra, $a0, 88
    move $a0, $zero
    jr $ra
.size ui_ft_save, . - ui_ft_save

.global ui_ft_restore
.type ui_ft_restore, @function
ui_ft_restore:
    ld.d $s0, $a0, 0
    ld.d $s1, $a0, 8
    ld.d $s2, $a0, 16
    ld.d $s3, $a0, 24
    ld.d $s4, $a0, 32
    ld.d $s5, $a0, 40
    ld.d $s6, $a0, 48
    ld.d $s7, $a0, 56
    ld.d $s8, $a0, 64
    ld.d $fp, $a0, 72
    ld.d $sp, $a0, 80
    ld.d $ra, $a0, 88
    li.w $a0, 1
    jr $ra
.size ui_ft_restore, . - ui_ft_restore
#endif
