# int 0x80 系统调用入口
#
# 当用户态执行 int $0x80 时:
#   1. CPU 从 TSS 加载 Ring0 栈 (ss0, esp0)
#   2. CPU 在 Ring0 栈上压入 ss, esp, eflags, cs, eip
#   3. 跳转到 isr128
#
# 栈布局 (从高到低):
#   [user_ss]     — 用户栈段 (仅从 Ring3 进入时有)
#   [user_esp]    — 用户栈指针
#   [eflags]      — 标志寄存器
#   [user_cs]     — 用户代码段
#   [user_eip]    — 用户程序下一条指令地址
#   [int_no]      — 我们 push 的 0x80
#   [err_code]    — 我们 push 的 0 (伪错误码)

.global isr128

isr128:
    # 伪错误码（保持与 registers_t 结构对齐）
    push $0
    # 压入中断号
    push $128

    # 保存所有通用寄存器
    pusha

    # 保存段寄存器
    push %ds
    push %es
    push %fs
    push %gs

    # 切换到内核数据段
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # 调用 C 处理函数
    push %esp           # 参数: registers_t *regs
    call syscall_handler
    add $4, %esp        # 恢复栈

    # 调用调度器（可能因 fork/exit/wait 需要切换任务）
    # 关键：在调用 scheduler_tick 前禁用中断，防止时钟中断
    # 在 scheduler_tick 返回后破坏 eax（popa 会覆盖 eax）。
    # iret 会恢复 eflags 中的 IF 位，自动重新启用中断。
    cli
    push %esp
    call scheduler_tick
    add $4, %esp
    mov %eax, %esp

    # 恢复段寄存器
    pop %gs
    pop %fs
    pop %es
    pop %ds

    # 恢复通用寄存器
    popa

    # 清除栈上的 int_no 和 err_code
    add $8, %esp

    iret
