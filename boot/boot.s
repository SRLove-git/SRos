/* boot.s - Multiboot compliant boot entry point
 *
 * 使用 GRUB Multiboot 协议引导，由 QEMU 的 -kernel 选项直接加载
 * 或通过 grub-rescue ISO 引导。
 *
 * 学习要点：
 * - Multiboot header 必须出现在 8K 以内
 * - EFLAGS 中的 IF 位初始为 0（中断关闭）
 * - ESP 需要在调用 C 代码前设置好
 */

.set ALIGN,     1 << 0
.set MEMINFO,   1 << 1
.set FLAGS,     ALIGN | MEMINFO
.set MAGIC,     0x1BADB002
.set CHECKSUM,  -(MAGIC + FLAGS)

.section .multiboot
    .align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .bss
    .align 16
    .globl stack_bottom
stack_bottom:
    .skip 16384
    .globl stack_top
stack_top:

.section .text
    .globl _start
_start:
    mov $stack_top, %esp

    /* 清零 BSS 段（确保全局变量初始为 0） */
    mov $_bss_start, %edi
    mov $_bss_end, %ecx
    sub %edi, %ecx
    xor %eax, %eax
    cld
    rep stosb

    call kernel_main

    /* 如果 kernel_main 返回，停机 */
    cli
1:  hlt
    jmp 1b
