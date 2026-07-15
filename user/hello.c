/*
 * hello.c — 示例用户程序
 *
 * 这是 SROS 上第一个独立加载的用户程序。
 * 不使用标准库，直接通过 int 0x80 系统调用与内核通信。
 *
 * 编译（通过 Makefile 自动完成）:
 *   x86_64-elf-gcc -m32 -ffreestanding -nostdlib \
 *       -T user/user.ld -o user/hello.elf user/hello.c
 */

/* 系统调用号（与 include/syscall.h 一致） */
#define SYS_WRITE   1
#define SYS_EXIT   14

static void putchar(char c)
{
    __asm__ volatile(
        "mov %0, %%ebx\n\t"
        "mov %1, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "r"((unsigned int)(unsigned char)c),
          "i"(SYS_WRITE)
        : "eax", "ebx"
    );
}

static void puts(const char *str)
{
    while (*str)
        putchar(*str++);
}

static void puthex(unsigned int val)
{
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    buf[10] = '\0';
    for (int i = 0; i < 8; i++) {
        int digit = (val >> (28 - i * 4)) & 0xF;
        buf[2 + i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
    }
    puts(buf);
}

static void putdec(unsigned int val)
{
    char buf[12];
    int pos = 11;
    buf[11] = '\0';
    if (val == 0) {
        puts("0");
        return;
    }
    while (val > 0 && pos > 0) {
        buf[--pos] = '0' + (val % 10);
        val /= 10;
    }
    puts(buf + pos);
}

void _start(void)
{
    puts("\n========================================\n");
    puts("  Hello from the FIRST user program!\n");
    puts("  This program was loaded by the SROS\n");
    puts("  ELF loader from the SFS filesystem.\n");
    puts("========================================\n\n");

    /* 打印一些诊断信息 */
    puts("Message from user space:\n");
    puts("  If you can see this, the ELF loader\n");
    puts("  is working correctly!\n\n");

    puts("Counting to 5...\n");
    for (int i = 1; i <= 5; i++) {
        puts("  ");
        putdec(i);
        puts("...\n");
    }
    puts("Blast off! ^_^\n");

    puts("\nExiting back to shell...\n");

    /* 退出 */
    __asm__ volatile(
        "mov %0, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "i"(SYS_EXIT)
        : "eax"
    );

    /* Never reached */
    while (1);
}
