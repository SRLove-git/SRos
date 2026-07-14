/* linker.ld - 链接脚本
 *
 * - 入口点必须与 boot.s 中的 _start 一致
 * - .multiboot 段必须放在最前（前 8K 内）
 * - QEMU 加载内核的默认起始地址是 0x100000 (1MB)
 */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
