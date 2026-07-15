/*
 * user.ld — 用户程序链接脚本
 *
 * 用户程序加载到虚拟地址 0x10000000 (256MB 处)，
 * 避免与内核 (0-4MB) 冲突。
 */

ENTRY(_start)

SECTIONS
{
    . = 0x10000000;

    .text : ALIGN(4K)
    {
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
