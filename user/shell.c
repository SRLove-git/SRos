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
        /* SYS_GETCHAR: 从键盘读取一个字符 */
        __asm__ volatile(
            "mov $2, %%eax\n\t"
            "int $0x80\n\t"
            : "=a"(c)
        );
        return c;
}
void putchar(char c){
    /* SYS_WRITE: 将字符打印到屏幕 */
        __asm__ volatile(
            "mov $1, %%eax\n\t"
            "mov %0, %%ebx\n\t"
            "int $0x80\n\t"
            :
            : "r"((u32)c)
            : "eax", "ebx"
        );
}
void cmd_help(int argc, char *argv[]){
    (void)argc; (void)argv;
    puts("help\n");
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
int parse_command(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;
    
    while (*p) {
        // 跳过前导空格
        while (*p == ' ') p++;
        if (*p == '\0') break;
        
        // 检查是否超过最大参数数
        if (argc >= max_args) break;
        
        // 记录单词开始
        argv[argc++] = p;
        
        // 找到单词结尾
        while (*p && *p != ' ') p++;
        
        // 用 \0 终止这个单词
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
            putchar('\n');           // 换行显示
            line[pos] = '\0';         // 结束字符串
            execute(line);            // 执行命令
            pos = 0;                  // 重置缓冲区
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
