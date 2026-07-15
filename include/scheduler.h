#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "idt.h"

#define TASK_NAME_MAX 32
#define MAX_TASKS 64

/* 任务状态 */
#define TASK_FREE    -1
#define TASK_READY    0
#define TASK_RUNNING  1
#define TASK_BLOCKED  2
#define TASK_ZOMBIE   3

/* 进程控制块 (PCB) */
typedef struct task {
    int pid;                           /* 进程 ID（-1 表示空闲槽位） */
    int parent_pid;                    /* 父进程 PID */
    volatile int state;                /* 任务状态 */
    int exit_code;                     /* 退出码（ZOMBIE 时有效） */
    u32 kernel_esp;                    /* 内核栈 ESP（指向 registers_t 帧） */
    u32 kernel_stack_base;             /* 内核栈底（释放用，0=不用释放） */
    u32 kernel_stack_top;              /* 内核栈顶（用于 TSS.ESP0） */
    u32 page_dir;                      /* 页目录 CR3（0 表示共享内核页表） */
    char name[TASK_NAME_MAX];          /* 任务名称 */
    struct task *next;                 /* 链表指针（就绪队列） */
} task_t;

/* 当前运行的进程 */
extern task_t *current_task;

/* 调度器初始化 */
void scheduler_init(void);

/* 时钟/系统调用后调度入口：保存当前上下文，返回下一个任务的寄存器帧指针 */
registers_t* scheduler_tick(registers_t *regs);

/* 创建新任务（用户态任务，入口在 Ring 3 执行） */
int task_create(const char *name, void (*entry)(void));

/* Fork：复制当前进程，返回子进程 PID（父进程中） */
int task_fork(registers_t *regs);

/* 退出当前进程（由 SYS_EXIT 调用）*/
void task_exit(int exit_code);

/* 等待子进程：pid=-1 表示任意子进程，返回子进程 PID 或 -1 */
int task_wait(int pid);

/* 主动让出 CPU */
void task_yield(void);

/* 按 PID 查找任务 */
task_t *task_from_pid(int pid);

#endif /* SCHEDULER_H */
