#include "shell.h"
#include "string.h"
#include "types.h"
#include "syscall.h"
#include "keyboard.h"
#include "mouse.h"

void puts(char *str){
    while(*str != '\0'){
        putchar(*str);
        str++;
    }
}
int getchar(void){
    int c;
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
        "int $0x80\n\t"
        :
        : "b"((u32)c)
        : "eax"
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

/* ===== 用户程序执行系统调用包装 ===== */

static int sys_exec(const char *path)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $13, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)path)
        : "ebx"
    );
    return ret;
}

/* ===== IPC 系统调用包装 ===== */

static int sys_sem_init(const char *name, int val)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov %2, %%ecx\n\t"
        "mov $17, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)name), "r"((u32)val)
        : "ebx", "ecx"
    );
    return ret;
}

static int sys_sem_wait(int sem_id)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $18, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)sem_id)
        : "ebx"
    );
    return ret;
}

static int sys_sem_post(int sem_id)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $19, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)sem_id)
        : "ebx"
    );
    return ret;
}

static int sys_sem_destroy(int sem_id)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $20, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)sem_id)
        : "ebx"
    );
    return ret;
}

static int sys_msg_get(void)
{
    int ret;
    __asm__ volatile(
        "mov $21, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        :
        :
    );
    return ret;
}

static int sys_msg_send(int mq_id, const char *buf, u32 len)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov %2, %%ecx\n\t"
        "mov %3, %%edx\n\t"
        "mov $22, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)mq_id), "r"((u32)buf), "r"((u32)len)
        : "ebx", "ecx", "edx"
    );
    return ret;
}

static int sys_msg_recv(int mq_id, char *buf, u32 *len)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov %2, %%ecx\n\t"
        "mov %3, %%edx\n\t"
        "mov $23, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)mq_id), "r"((u32)buf), "r"((u32)len)
        : "ebx", "ecx", "edx"
    );
    return ret;
}

static int sys_msg_destroy(int mq_id)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $24, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)mq_id)
        : "ebx"
    );
    return ret;
}

/* ===== 进程控制系统调用包装 ===== */

static int sys_fork(void)
{
    int pid;
    __asm__ volatile(
        "mov $15, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(pid)
        :
        :
    );
    return pid;
}

static void sys_exit(int code)
{
    __asm__ volatile(
        "mov %0, %%ebx\n\t"
        "mov $14, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "r"((u32)code)
        : "eax", "ebx"
    );
}

static int sys_wait(int pid)
{
    int ret;
    __asm__ volatile(
        "mov %1, %%ebx\n\t"
        "mov $16, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        : "r"((u32)pid)
        : "ebx"
    );
    return ret;
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
    puts("  exec <file>         - Execute a user program\n");
    puts("  fork_test           - Test fork/wait/exit\n");
    puts("  sem_test            - Test semaphore (create/wait/post/destroy)\n");
    puts("  msg_test            - Test message queue (send/recv)\n");
    puts("  mouse               - 实时显示鼠标事件\n");
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
/* ===== 鼠标系统调用包装 ===== */

static int sys_mouse_has_data(void)
{
    int ret;
    __asm__ volatile(
        "mov $25, %%eax\n\t"
        "int $0x80\n\t"
        : "=a"(ret)
        :
        :
    );
    return ret;
}

static void sys_mouse_read(mouse_event_t *ev)
{
    __asm__ volatile(
        "mov %0, %%ebx\n\t"
        "mov $26, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "r"((u32)ev)
        : "eax", "ebx"
    );
}

/* ===== 鼠标实时显示命令 ===== */

static void mouse_format_buttons(u8 btns, char *buf)
{
    buf[0] = (btns & MOUSE_LEFT_BTN)   ? 'L' : '-';
    buf[1] = (btns & MOUSE_MIDDLE_BTN) ? 'M' : '-';
    buf[2] = (btns & MOUSE_RIGHT_BTN)  ? 'R' : '-';
    buf[3] = '\0';
}

void cmd_mouse(int argc, char *argv[])
{
    (void)argc; (void)argv;
    puts("Mouse monitor: move/click to see events. Press any key to exit.\n");
    puts("  dx   dy   buttons\n");

    char btn_str[4];
    while (!kbd_buffer_has_data()) {
        if (!sys_mouse_has_data()) continue;

        mouse_event_t ev;
        sys_mouse_read(&ev);

        mouse_format_buttons(ev.buttons, btn_str);

        /* 用空格分隔，\r 回到行首覆盖显示 */
        putchar('\r');
        putchar(' ');

        /* dx */
        if (ev.dx >= 0) putchar(' ');
        putchar(' ');
        if (ev.dx < -9 || ev.dx > 99) /* 三位数 */ ;
        else if (ev.dx < -9 || ev.dx > 9) putchar(' ');
        else putchar(' ');
        puthex((u8)ev.dx);
        putchar(' ');

        /* dy */
        if (ev.dy >= 0) putchar(' ');
        putchar(' ');
        if (ev.dy < -9 || ev.dy > 99) ;
        else if (ev.dy < -9 || ev.dy > 9) putchar(' ');
        else putchar(' ');
        puthex((u8)ev.dy);
        putchar(' ');

        puts(btn_str);
        putchar(' ');

        /* 读取按键值，便于检查 */
        if (ev.buttons) {
            putchar('(');
            if (ev.buttons & MOUSE_LEFT_BTN)   putchar('L');
            if (ev.buttons & MOUSE_RIGHT_BTN)  putchar('R');
            if (ev.buttons & MOUSE_MIDDLE_BTN) putchar('M');
            putchar(')');
        }
    }

    /* 吃掉退出前残留的键盘输入 */
    putchar('\n');
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

void cmd_exec(int argc, char *argv[])
{
    if (argc < 2) {
        puts("Usage: exec <filename>\n");
        return;
    }

    /* 构建路径：/ + 文件名 */
    char path[64];
    path[0] = '/';
    path[1] = '\0';
    strcpy(path + 1, argv[1]);

    puts("Loading: ");
    puts(argv[1]);
    puts("\n");

    int ret = sys_exec(path);
    if (ret < 0) {
        puts("Error: Failed to load program\n");
    }
    /* 成功时不返回 */
}

/* ===== Fork/Exit/Wait 测试命令 ===== */

static void cmd_fork_test(int argc, char *argv[])
{
    (void)argc; (void)argv;

    puts("Fork test: about to fork...\n");

    int pid = sys_fork();
    if (pid < 0) {
        puts("Fork failed!\n");
        return;
    }

    if (pid == 0) {
        /* 子进程 — 输出子进程 PID 确认 */
        puts("Child process: about to exit...\n");
        sys_exit(42);
        /* 不应到达这里 — 如果到达说明 sys_exit 返回了！ */
        puts("[BUG] Child: sys_exit returned to user mode!\n");
        /* 死循环防止继续执行 */
        for (;;) {
            __asm__ volatile("hlt");
        }
    } else {
        /* 父进程 */
        puts("Parent process: PID=");
        puthex(pid);
        puts("\nParent: waiting for child...\n");

        int ret = sys_wait(-1);
        puts("Parent: wait returned PID=");
        puthex(ret);
        puts("\nParent: done.\n");
    }
}

/* ===== 信号量测试命令 ===== */

static void cmd_sem_test(int argc, char *argv[])
{
    (void)argc; (void)argv;

    puts("Semaphore test: creating semaphore with initial value 1\n");
    int sem_id = sys_sem_init("test_sem", 1);
    if (sem_id < 0) {
        puts("Failed to create semaphore!\n");
        return;
    }
    puts("Semaphore created, id=");
    puthex(sem_id);
    putchar('\n');

    puts("Waiting (P operation)...\n");
    sys_sem_wait(sem_id);
    puts("Acquired semaphore (value was 1, now 0)\n");

    int child_pid = sys_fork();
    if (child_pid == 0) {
        /* 子进程尝试获取信号量——会被阻塞 */
        puts("Child: trying to wait on semaphore...\n");
        sys_sem_wait(sem_id);
        puts("Child: acquired semaphore!\n");
        sys_sem_post(sem_id);
        puts("Child: posted semaphore, exiting.\n");
        sys_exit(0);
    } else {
        /* 父进程持有信号量，延迟后释放 */
        puts("Parent: holding semaphore for a while...\n");
        for (volatile int i = 0; i < 10000000; i++);
        puts("Parent: posting semaphore...\n");
        sys_sem_post(sem_id);
        puts("Parent: waiting for child...\n");
        sys_wait(-1);
        sys_sem_destroy(sem_id);
        puts("Semaphore test done.\n");
    }
}

/* ===== 消息队列测试命令 ===== */

static void cmd_msg_test(int argc, char *argv[])
{
    (void)argc; (void)argv;

    puts("Message queue test: creating queue\n");
    int mq_id = sys_msg_get();
    if (mq_id < 0) {
        puts("Failed to create message queue!\n");
        return;
    }
    puts("Message queue created, id=");
    puthex(mq_id);
    putchar('\n');

    int child_pid = sys_fork();
    if (child_pid == 0) {
        /* 子进程：接收消息 */
        char buf[64];
        u32 len;
        puts("Child: waiting for message...\n");
        sys_msg_recv(mq_id, buf, &len);
        buf[len] = '\0';
        puts("Child: received '");
        puts(buf);
        puts("'\n");
        sys_exit(0);
    } else {
        /* 父进程：发送消息 */
        const char *msg = "Hello from parent!";
        puts("Parent: sending message...\n");
        sys_msg_send(mq_id, msg, strlen(msg));
        puts("Parent: waiting for child...\n");
        sys_wait(-1);
        sys_msg_destroy(mq_id);
        puts("Message queue test done.\n");
    }
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
    else if (strcmp(argv[0], "exec") == 0) {
        cmd_exec(argc, argv);
    }
    else if (strcmp(argv[0], "fork_test") == 0) {
        cmd_fork_test(argc, argv);
    }
    else if (strcmp(argv[0], "sem_test") == 0) {
        cmd_sem_test(argc, argv);
    }
    else if (strcmp(argv[0], "msg_test") == 0) {
        cmd_msg_test(argc, argv);
    }
    else if (strcmp(argv[0], "mouse") == 0) {
        cmd_mouse(argc, argv);
    }
    else {
        puts("Error: Unknown command\n");
    }
}

/* ===== 命令历史 ===== */
#define HISTORY_MAX 16
static char history[HISTORY_MAX][256];
static int history_count = 0;

static void history_add(const char *cmd)
{
    if (strlen(cmd) == 0) return;

    /* 不重复添加连续相同的命令 */
    if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0)
        return;

    /* 如果满了，整体前移扔掉最旧的 */
    if (history_count >= HISTORY_MAX) {
        for (int i = 1; i < HISTORY_MAX; i++)
            strcpy(history[i - 1], history[i]);
        history_count = HISTORY_MAX - 1;
    }

    strcpy(history[history_count], cmd);
    history_count++;
}

/* 从屏幕上清除 len 个字符 */
static void clear_input(int len)
{
    while (len-- > 0)
        putchar('\b');
}

void shell_main(void){
    char line[256];
    int  pos = 0;
    int  line_len = 0;
    static char saved_line[256];
    static int saved_pos = 0;
    int history_index = -1;   /* -1 = 当前输入, 0..n = 历史 */
    puts("SRos> ");
    while (1) {
        int c = getchar();
        if (c == '\n') {
            putchar('\n');
            line[pos] = '\0';
            history_add(line);
            execute(line);
            pos = 0;
            line_len = 0;
            history_index = -1;
            puts("SRos> ");
        }
        else if (c == '\b') {
            if (pos > 0) {
                pos--;
                line_len--;
                putchar('\b');   /* 光标左移，在删除位置写空格 */

                /* 将光标后的字符逐一左移 */
                for (int i = pos; i < line_len; i++)
                    line[i] = line[i + 1];
                line[line_len] = '\0';

                /* 重新绘制从光标到行尾的内容 */
                for (int i = pos; i < line_len; i++)
                    putchar(line[i]);
                putchar(' ');    /* 清除行尾旧字符 */

                /* 将光标移回编辑位置 */
                for (int i = pos; i < line_len + 1; i++)
                    putchar(CURSOR_LEFT);
            }
        }
        else if (c == KEY_UP) {
            if (history_count == 0) continue;
            if (history_index == -1) {
                strcpy(saved_line, line);
                saved_pos = pos;
            }
            if (history_index < history_count - 1) {
                history_index++;
                clear_input(pos);
                strcpy(line, history[history_count - 1 - history_index]);
                pos = strlen(line);
                line_len = pos;
                puts(line);
            }
            continue;
        }
        else if (c == KEY_DOWN) {
            if (history_index == -1) continue;
            if (history_index > 0) {
                history_index--;
                clear_input(pos);
                strcpy(line, history[history_count - 1 - history_index]);
                pos = strlen(line);
                line_len = pos;
                puts(line);
            }
            else {
                history_index = -1;
                clear_input(pos);
                strcpy(line, saved_line);
                pos = saved_pos;
                line_len = pos;
                puts(line);
            }
            continue;
        }
        else if (c == KEY_LEFT) {
            if (pos > 0) {
                pos--;
                putchar(CURSOR_LEFT);
            }
            continue;
        }
        else if (c == KEY_RIGHT) {
            if (pos < line_len) {
                putchar(CURSOR_RIGHT);
                pos++;
            }
            continue;
        }
        else if (pos < 255) {
            line[pos++] = c;
            if (pos > line_len) line_len = pos;
            putchar(c);
        }
    }
}
