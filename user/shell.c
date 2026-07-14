#include "shell.h"
#include "string.h"
#include "types.h"
#include "syscall.h"

void puts(char *str){
    while(*str != '\0'){
        putchar(*str);
        str++;
    }
}
char getchar(void){
    char c;
    __asm__ volatile(
        "mov $2, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(c)
    );
    return c;
}
void putchar(char c){
    __asm__ volatile(
        "mov $1, %%eax\n\t"
        "mov %0, %%ebx\n\t"
        "int $0x80\n\t"
        :
        : "r"((u32)c)
        : "eax", "ebx"
    );
}

/* ===== 文件系统系统调用包装函数 ===== */

int fs_open(const char *path)
{
    int fd;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $4, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(fd)
        : "r"((u32)path)
        : "ebx"
    );
    return fd;
}

int fs_read(int fd, char *buf, int count)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov %2, %%ecx\n\t"
        "mov %3, %%edx\n\t"
        "mov $5, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)fd), "r"((u32)buf), "r"((u32)count)
        : "ebx", "ecx", "edx"
    );
    return ret;
}

int fs_write(int fd, const char *buf, int count)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov %2, %%ecx\n\t"
        "mov %3, %%edx\n\t"
        "mov $6, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)fd), "r"((u32)buf), "r"((u32)count)
        : "ebx", "ecx", "edx"
    );
    return ret;
}

void fs_close(int fd)
{
    __asm__ volatile(
        "mov %0, %%ebx\n\t"
        "mov $7, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "r"((u32)fd)
        : "eax", "ebx"
    );
}

int fs_create(const char *name)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $8, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)name)
        : "ebx"
    );
    return ret;
}

void fs_ls(void)
{
    char buf[512];
    int n;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov %2, %%ecx\n\t"
        "mov $9, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(n)
        : "r"((u32)buf), "r"((u32)sizeof(buf))
        : "ebx", "ecx"
    );
    if (n > 0) {
        buf[n] = '\0';
        puts(buf);
        putchar('\n');
    }
}

/* ===== 堆内存系统调用包装函数 ===== */

static void *sys_kmalloc(u32 size)
{
    void *ptr;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $10, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ptr)
        : "r"((u32)size)
        : "ebx"
    );
    return ptr;
}

static void sys_kfree(void *ptr)
{
    __asm__ volatile(
        "mov %0, %%ebx\n\t"
        "mov $11, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "r"((u32)ptr)
        : "eax", "ebx"
    );
}

static void sys_kmdump(void)
{
    __asm__ volatile(
        "mov $12, %%eax\n\t"
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
}

/* ===== 辅助函数 ===== */

static void puthex(u32 val)
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

static u32 hextoi(char *s)
{
    u32 val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    while (*s) {
        val <<= 4;
        if (*s >= '0' && *s <= '9')
            val |= *s - '0';
        else if (*s >= 'a' && *s <= 'f')
            val |= *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F')
            val |= *s - 'A' + 10;
        else
            break;
        s++;
    }
    return val;
}

/* ===== Shell 命令 ===== */

void cmd_kmalloc(int argc, char *argv[])
{
    if (argc < 2) {
        puts("Usage: kmalloc <size>\n");
        return;
    }

    u32 size = 0;
    char *s = argv[1];
    while (*s) {
        if (*s < '0' || *s > '9') {
            puts("Invalid size\n");
            return;
        }
        size = size * 10 + (*s - '0');
        s++;
    }

    void *ptr = sys_kmalloc(size);
    puts("kmalloc(");
    puts(argv[1]);
    puts(") = ");
    puthex((u32)ptr);
    putchar('\n');
}

void cmd_kfree(int argc, char *argv[])
{
    if (argc < 2) {
        puts("Usage: kfree <hexaddr>\n");
        puts("Example: kfree 0x200004\n");
        return;
    }

    u32 addr = hextoi(argv[1]);
    sys_kfree((void *)addr);
    puts("freed ");
    puthex(addr);
    putchar('\n');
}

void cmd_kmdump(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sys_kmdump();
}

void cmd_help(int argc, char *argv[]){
    (void)argc; (void)argv;
    puts("Available commands:\n");
    puts("  help           - Show this help\n");
    puts("  echo <text>    - Print text\n");
    puts("  clear          - Clear screen\n");
    puts("  ls             - List files\n");
    puts("  cat <file>     - Show file content\n");
    puts("  touch <file>   - Create a file\n");
    puts("  write <file> <text> - Write text to file\n");
    puts("  kmalloc <size>      - Allocate heap memory\n");
    puts("  kfree <hexaddr>     - Free heap memory\n");
    puts("  kmdump              - Dump heap state\n");
}
void cmd_echo(int argc, char *argv[]){
    if (argc == 1) {
        putchar('\n');
    }
    else {
        for (int i = 1; i < argc; i++) {
            puts(argv[i]);
            if (i < argc - 1) putchar(' ');
        }
        putchar('\n');
    }
}
void cmd_clear(int argc, char *argv[]){
    (void)argc; (void)argv;
    __asm__ volatile(
        "mov $3, %%eax\n\t"
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
}

void cmd_ls(int argc, char *argv[]){
    (void)argc; (void)argv;
    fs_ls();
}

void cmd_cat(int argc, char *argv[]){
    if (argc < 2) {
        puts("Usage: cat <filename>\n");
        return;
    }

    /* 构建路径：/ + 文件名 */
    char path[64];
    path[0] = '/';
    path[1] = '\0';
    strcpy(path + 1, argv[1]);

    int fd = fs_open(path);
    if (fd < 0) {
        puts("File not found: ");
        puts(argv[1]);
        putchar('\n');
        return;
    }

    char buf[256];
    int n;
    while ((n = fs_read(fd, buf, 255)) > 0) {
        buf[n] = '\0';
        puts(buf);
    }
    putchar('\n');
    fs_close(fd);
}

void cmd_touch(int argc, char *argv[]){
    if (argc < 2) {
        puts("Usage: touch <filename>\n");
        return;
    }

    if (fs_create(argv[1]) == 0)
        puts("Created: ");
    else
        puts("Failed: ");
    puts(argv[1]);
    putchar('\n');
}

void cmd_write(int argc, char *argv[]){
    if (argc < 3) {
        puts("Usage: write <filename> <text>\n");
        return;
    }

    /* 先尝试创建（如果文件已存在则失败，没关系） */
    fs_create(argv[1]);

    /* 打开文件 */
    char path[64];
    path[0] = '/';
    path[1] = '\0';
    strcpy(path + 1, argv[1]);

    int fd = fs_open(path);
    if (fd < 0) {
        puts("Cannot open: ");
        puts(argv[1]);
        putchar('\n');
        return;
    }

    /* 写入文本 */
    fs_write(fd, argv[2], strlen(argv[2]));
    fs_close(fd);

    puts("Written to: ");
    puts(argv[1]);
    putchar('\n');
}

int parse_command(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ') p++;
        if (*p == '\0') break;
        if (argc >= max_args) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }

    return argc;
}
void execute(char *line){
    char *argv[16];
    int argc = parse_command(line, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc == 0) {
        return;
    }
    if (strcmp(argv[0], "echo") == 0) {
        cmd_echo(argc, argv);
    }
    else if (strcmp(argv[0], "clear") == 0) {
        cmd_clear(argc, argv);
    }else if(strcmp(argv[0], "help") == 0){
        cmd_help(argc, argv);
    }else if(strcmp(argv[0], "ls") == 0){
        cmd_ls(argc, argv);
    }else if(strcmp(argv[0], "cat") == 0){
        cmd_cat(argc, argv);
    }else if(strcmp(argv[0], "touch") == 0){
        cmd_touch(argc, argv);
    }else if(strcmp(argv[0], "write") == 0){
        cmd_write(argc, argv);
    }else if(strcmp(argv[0], "kmalloc") == 0){
        cmd_kmalloc(argc, argv);
    }else if(strcmp(argv[0], "kfree") == 0){
        cmd_kfree(argc, argv);
    }else if(strcmp(argv[0], "kmdump") == 0){
        cmd_kmdump(argc, argv);
    }
    else {
        puts("Error: Unknown command\n");
    }
}

void shell_main(void){
    char line[256];
    int  pos = 0;
    puts("SRos> ");
    while (1) {
        char c = getchar();
        if (c == '\n') {
            putchar('\n');
            line[pos] = '\0';
            execute(line);
            pos = 0;
            puts("SRos> ");
        }
        else if (c == '\b') {
            if (pos > 0) {
                pos--;
                putchar('\b');
            }
        }
        else if (pos < 255) {
            line[pos++] = c;
            putchar(c);
        }
    }
}
