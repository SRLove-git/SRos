#include "scheduler.h"
#include "gdt.h"
#include "paging.h"
#include "serial.h"
#include "string.h"
#include "vga.h"

/* 用户栈配置（与 ELF loader 保持一致） */
#define USER_STACK_VADDR    0xE0000000

/* ===== 进程表与就绪队列 ===== */
static task_t task_table[MAX_TASKS];
static int task_count = 0;
static int next_pid = 0;

task_t *current_task = NULL;

/* 就绪队列（FIFO 单链表） */
static task_t *ready_head = NULL;
static task_t *ready_tail = NULL;

static void enqueue_ready(task_t *task)
{
    task->next = NULL;
    if (ready_tail) {
        ready_tail->next = task;
    } else {
        ready_head = task;
    }
    ready_tail = task;
}

static task_t *dequeue_ready(void)
{
    if (!ready_head) return NULL;
    task_t *task = ready_head;
    ready_head = task->next;
    if (!ready_head)
        ready_tail = NULL;
    task->next = NULL;
    return task;
}

/* ===== 调度器初始化 ===== */
void scheduler_init(void)
{
    serial_printf(SERIAL_COM1, "[sched] Initializing scheduler...\n");

    /* 初始化所有任务槽位为 free */
    for (int i = 0; i < MAX_TASKS; i++)
        task_table[i].pid = -1;

    /* Task 0：当前正在运行的 Shell / init 任务 */
    task_t *task = &task_table[0];
    task->pid = next_pid++;
    task->parent_pid = -1;
    task->state = TASK_RUNNING;
    task->exit_code = 0;
    task->kernel_esp = 0;                      /* 首次时钟中断时捕获 */
    task->kernel_stack_base = 0;               /* 引导栈，不释放 */
    task->kernel_stack_top = 0x20000;          /* 当前 TSS.ESP0 */
    __asm__ volatile("mov %%cr3, %0" : "=r"(task->page_dir));
    strcpy(task->name, "init");
    task->next = NULL;

    current_task = task;
    task_count = 1;

    /* 更新 TSS 指向当前任务的内核栈 */
    tss.esp0 = task->kernel_stack_top;

    serial_printf(SERIAL_COM1, "[sched] Task 0 (init) ready, CR3=0x%x\n",
                  task->page_dir);
}

/* ===== 调度入口 =====
 * 只在时钟中断 (IRQ0 = 向量 32) 时触发调度。
 * 系统调用返回 (int 0x80 = 向量 128) 路径不做调度切换，
 * SYS_EXIT 在 task_exit 中直接切换，SYS_WAIT 通过 int $32 阻塞。 */
registers_t* scheduler_tick(registers_t *regs)
{
    /* 只在时钟中断 (IRQ0 = 向量 32) 时触发调度 */
    if (regs->int_no != 32)
        return regs;

    extern volatile u32 timer_ticks;
    timer_ticks++;

    if (!current_task) return regs;

    /* 保存当前任务上下文 */
    current_task->kernel_esp = (u32)regs;

    /* ZOMBIE 任务：不入就绪队列，直接选下一个 */
    if (current_task->state == TASK_ZOMBIE) {
        goto pick_next;
    }

    /* BLOCKED 任务：不入就绪队列 */
    if (current_task->state == TASK_BLOCKED) {
        goto pick_next;
    }

    /* READY/RUNNING：入就绪队列（只在多任务时） */
    if (task_count > 1) {
        current_task->state = TASK_READY;
        enqueue_ready(current_task);
    } else {
        /* 只有一个任务，无需切换 */
        current_task->state = TASK_RUNNING;
        return regs;
    }

pick_next:
    {
        task_t *next = dequeue_ready();
        if (!next) {
            if (current_task->state != TASK_ZOMBIE) {
                current_task->state = TASK_RUNNING;
                return regs;
            }
            /* 所有任务都 zombie/blocked，停机 */
            serial_printf(SERIAL_COM1, "[sched] No runnable tasks!\n");
            __asm__ volatile("cli; hlt");
            return regs;
        }

        serial_printf(SERIAL_COM1, "[sched] switch %d -> %d (esp 0x%x)\n",
                      current_task->pid, next->pid, next->kernel_esp);

        current_task = next;
        current_task->state = TASK_RUNNING;

        /* 更新 TSS.ESP0，确保中断从 Ring 3 进入时使用新任务的内核栈 */
        tss.esp0 = current_task->kernel_stack_top;

        /* 如果新任务有自己的页目录，切换地址空间 */
        if (current_task->page_dir) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(current_task->page_dir)
                             : "memory");
        }

        /* 返回新任务的寄存器帧指针，恢复新任务的上下文 */
        return (registers_t *)current_task->kernel_esp;
    }
}

/* ===== 查找空闲槽位 ===== */
static int task_find_slot(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid < 0)
            return i;
    }
    return -1;
}

/* ===== 创建新任务 ===== */
int task_create(const char *name, void (*entry)(void))
{
    int slot = task_find_slot();
    if (slot < 0) {
        serial_printf(SERIAL_COM1, "[sched] Max tasks reached!\n");
        return -1;
    }

    /* 1. 分配内核栈（1 页，identity-mapped） */
    u32 kstack_phys = pfa_alloc();
    u32 kstack_top  = kstack_phys + 0x1000;

    /* 2. 在内核栈顶设置初始 registers_t 帧（用户态上下文） */
    registers_t *frame = (registers_t *)(kstack_top - sizeof(registers_t));
    memset(frame, 0, sizeof(registers_t));

    frame->gs       = 0x23;      /* 用户数据段，RPL=3 */
    frame->fs       = 0x23;
    frame->es       = 0x23;
    frame->ds       = 0x23;

    /* pusha 恢复时 ESP 应指向 int_no 域（即进入中断时的 ESP） */
    frame->esp      = (u32)(&frame->int_no);

    /* eip / cs：跳转到用户态入口 */
    frame->eip      = (u32)entry;
    frame->cs       = 0x1B;      /* 用户代码段，RPL=3 */
    frame->eflags   = 0x200;     /* IF=1 */

    /* 确保入口代码的用户态可访问性（设置页目录 User 位） */
    paging_map_user((u32)entry);

    /* 3. 分配并映射用户栈（1 页 = 4KB） */
    u32 user_stack_phys = pfa_alloc();
    paging_map_page(USER_STACK_VADDR - 0x1000, user_stack_phys, 0x07);

    frame->user_esp = USER_STACK_VADDR;   /* 栈顶（向下增长） */
    frame->user_ss  = 0x23;
    /* 4. 填充 PCB */
    task_t *task = &task_table[slot];
    task->pid             = next_pid++;
    task->parent_pid      = current_task ? current_task->pid : -1;
    task->state           = TASK_READY;
    task->exit_code       = 0;
    task->kernel_esp      = (u32)frame;
    task->kernel_stack_base = kstack_phys;
    task->kernel_stack_top = kstack_top;
    task->page_dir        = 0;   /* 共享内核页表 */
    strcpy(task->name, name);
    task->next            = NULL;

    /* 5. 加入就绪队列 */
    enqueue_ready(task);
    task_count++;

    serial_printf(SERIAL_COM1, "[sched] Task %d (%s) created, entry=0x%x, "
                  "kstack=0x%x\n", task->pid, name, (u32)entry, kstack_top);

    return task->pid;
}

/* ===== 主动让出 CPU ===== */
void task_yield(void)
{
    /* 触发时钟中断，从而调用 scheduler_tick 进行任务切换 */
    __asm__ volatile("int $32");
}

/* ===== 按 PID 查找任务 ===== */
task_t *task_from_pid(int pid)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == pid)
            return &task_table[i];
    }
    return NULL;
}

/* ===== Fork：复制当前进程 ===== */
int task_fork(registers_t *regs)
{
    /* 1. 查找空闲槽位 */
    int slot = task_find_slot();
    if (slot < 0) {
        serial_printf(SERIAL_COM1, "[sched] fork: no slot!\n");
        return -1;
    }

    /* 2. 克隆页目录 */
    u32 new_page_dir = paging_clone_directory();
    if (!new_page_dir) {
        serial_printf(SERIAL_COM1, "[sched] fork: clone page dir failed!\n");
        return -1;
    }

    /* 3. 分配新的内核栈 */
    u32 kstack_phys = pfa_alloc();
    u32 kstack_top  = kstack_phys + 0x1000;

    /* 4. 复制寄存器帧到新内核栈顶 */
    registers_t *child_frame = (registers_t *)(kstack_top - sizeof(registers_t));
    memcpy(child_frame, regs, sizeof(registers_t));

    /* 5. 子进程返回 0 */
    child_frame->eax = 0;

    /* 6. 填充 PCB */
    task_t *child = &task_table[slot];
    child->pid               = next_pid++;
    child->parent_pid        = current_task->pid;
    child->state             = TASK_READY;
    child->exit_code         = 0;
    child->kernel_esp        = (u32)child_frame;
    child->kernel_stack_base = kstack_phys;
    child->kernel_stack_top  = kstack_top;
    child->page_dir          = new_page_dir;
    strcpy(child->name, current_task->name);
    child->next              = NULL;

    /* 7. 子进程入就绪队列 */
    enqueue_ready(child);
    task_count++;

    int child_pid = child->pid;

    serial_printf(SERIAL_COM1, "[sched] fork: child %d (parent %d), "
                  "kstack=0x%x, pgdir=0x%x\n",
                  child_pid, current_task->pid, kstack_top, new_page_dir);

    return child_pid;
}

/* ===== Exit：退出当前进程 ===== */
void task_exit(int exit_code)
{
    if (!current_task) return;

    int pid = current_task->pid;
    serial_printf(SERIAL_COM1, "[sched] task %d (%s) exiting with code %d\n",
                  pid, current_task->name, exit_code);

    current_task->exit_code = exit_code;
    current_task->state = TASK_ZOMBIE;

    /* 如果有父进程在等待，唤醒它 */
    if (current_task->parent_pid >= 0) {
        task_t *parent = task_from_pid(current_task->parent_pid);
        if (parent && parent->state == TASK_BLOCKED) {
            parent->state = TASK_READY;
            enqueue_ready(parent);
            serial_printf(SERIAL_COM1, "[sched] woke up parent %d\n",
                          parent->pid);
        }
    }

    /* 触发调度：让出 CPU。scheduler_tick 在处理 int $32 时会
     * 检测到当前任务 ZOMBIE，直接选下一个就绪任务。 */
    __asm__ volatile("int $32");

    /* 不应到达这里——ZOMBIE 任务不会被重新调度 */
    serial_printf(SERIAL_COM1, "[sched] ERROR: ZOMBIE task %d resumed!\n", pid);
    __asm__ volatile("cli; hlt");
    for (;;);
}

/* ===== Wait：等待子进程退出 ===== */
int task_wait(int pid)
{
    if (!current_task) return -1;

    serial_printf(SERIAL_COM1, "[sched] task %d waiting for child pid=%d\n",
                  current_task->pid, pid);

    /* 循环直到找到 ZOMBIE 子进程 */
    while (1) {
        /* 遍历查找符合条件（pid=-1 任意子进程，否则特定 PID）的子进程 */
        for (int i = 0; i < MAX_TASKS; i++) {
            task_t *child = &task_table[i];
            if (child->pid < 0) continue;
            if (child->parent_pid != current_task->pid) continue;
            if (pid >= 0 && child->pid != pid) continue;

            if (child->state == TASK_ZOMBIE) {
                /* 找到退出的子进程，收集退出码 */
                int child_pid = child->pid;
                int exit_code = child->exit_code;

                /* 释放子进程资源 */
                if (child->kernel_stack_base) {
                    pfa_free(child->kernel_stack_base);
                }
                if (child->page_dir) {
                    /* 简化处理：不递归释放页目录占用的所有页，
                     * 直接释放页目录所在的物理页 */
                    pfa_free(child->page_dir);
                }

                /* 清空 PCB 槽位 */
                memset(child, 0, sizeof(task_t));
                child->pid = -1;
                task_count--;

                serial_printf(SERIAL_COM1, "[sched] wait: collected child %d "
                              "(exit=%d)\n", child_pid, exit_code);
                return child_pid;
            }
        }

        /* 检查是否还有存活的子进程（非 ZOMBIE） */
        int has_alive = 0;
        for (int i = 0; i < MAX_TASKS; i++) {
            task_t *child = &task_table[i];
            if (child->pid < 0) continue;
            if (child->parent_pid != current_task->pid) continue;
            if (pid >= 0 && child->pid != pid) continue;
            if (child->state != TASK_ZOMBIE) {
                has_alive = 1;
                break;
            }
        }

        if (!has_alive) {
            /* 没有子进程了，返回 -1 */
            serial_printf(SERIAL_COM1, "[sched] wait: no children left\n");
            return -1;
        }

        /* 有存活子进程但还没退出，阻塞当前进程 */
        current_task->state = TASK_BLOCKED;

        /* 触发调度：切换到其他任务 */
        __asm__ volatile("int $32");

        /* 被唤醒后继续循环检查 */
    }
}
