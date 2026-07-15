# 用宏定义每个异常的入口
.macro ISR_NOERRCODE vector
.global isr\vector

isr\vector:
    push $0              # 伪错误码（占位，保持栈统一）
    push $\vector        # 压入中断号
    jmp isr_common       # 跳转到公共处理
.endm

.macro ISR_ERRCODE vector
.global isr\vector
isr\vector:
    push $\vector        # 压入中断号（错误码已经在栈上）
    jmp isr_common
.endm
.macro IRQ_NOERRCODE vector
.global isr\vector
isr\vector:
    push $0
    push $\vector
    jmp irq_common
.endm
isr_common:
    # 保存所有寄存器（通用寄存器 + 段寄存器）
    pusha                # EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    # 可选：保存段寄存器
    push  %ds
    push  %es
    push  %fs
    push  %gs

    # 切换到内核数据段
    mov   $0x10, %ax     # GDT 数据段选择子
    mov   %ax, %ds
    mov   %ax, %es
    mov   %ax, %fs
    mov   %ax, %gs

    # 调用 C 处理函数
    # 栈上自顶向下：gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax, int_no, err_code
    push  %esp           # 传递寄存器帧指针
    call  isr_handler    # void isr_handler(registers_t *regs)
    add   $4, %esp       # 清理 push %esp

    # 恢复段寄存器
    pop   %gs
    pop   %fs
    pop   %es
    pop   %ds

    # 恢复通用寄存器
    popa

    # 清除栈上的 int_no 和 err_code
    add   $8, %esp

    # 中断返回
    iret
irq_common:
    # 保存所有寄存器（通用寄存器 + 段寄存器）
    pusha                # EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    # 可选：保存段寄存器
    push  %ds
    push  %es
    push  %fs
    push  %gs

    # 切换到内核数据段
    mov   $0x10, %ax     # GDT 数据段选择子
    mov   %ax, %ds
    mov   %ax, %es
    mov   %ax, %fs
    mov   %ax, %gs

    # 调用 C 处理函数
    # 栈上自顶向下：gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax, int_no, err_code
    push  %esp           # 传递寄存器帧指针
    call  irq_handler    # void irq_handler(registers_t *regs)
    add   $4, %esp       # 清理 push %esp

    # 调用调度器（可能切换任务，返回新任务的 regs 指针）
    push  %esp
    call  scheduler_tick # registers_t* scheduler_tick(registers_t *regs)
    add   $4, %esp
    mov   %eax, %esp     # 切换到新任务的内核栈！

    # 恢复段寄存器
    pop   %gs
    pop   %fs
    pop   %es
    pop   %ds

    # 恢复通用寄存器
    popa

    # 清除栈上的 int_no 和 err_code
    add   $8, %esp

    # 中断返回
    iret


# 生成具体入口
ISR_NOERRCODE 0    # #DE 除零
ISR_NOERRCODE 1    # #DB 调试
ISR_NOERRCODE 3    # #BP 断点
ISR_NOERRCODE 4    # #OF 溢出
ISR_NOERRCODE 6    # #UD 无效指令
ISR_NOERRCODE 7    # #NM FPU 不可用
ISR_ERRCODE 8      # #DF 双重错误
ISR_ERRCODE 10     # #TS 无效 TSS
ISR_ERRCODE 11     # #NP 段不存在
ISR_ERRCODE 12     # #SS 栈段错误
ISR_ERRCODE 13     # #GP 通用保护
ISR_ERRCODE 14     # #PF 页错误
ISR_NOERRCODE 16   # #MF FPU 错误
ISR_NOERRCODE 17   # #AC 对齐检查
ISR_NOERRCODE 18   # #MC Machine Check
ISR_NOERRCODE 19   # #XM SIMD 异常
# IRQ 入口 (向量号 32~47)
IRQ_NOERRCODE 32   # IRQ0: PIT 定时器
IRQ_NOERRCODE 33   # IRQ1: 键盘
IRQ_NOERRCODE 34
IRQ_NOERRCODE 35
IRQ_NOERRCODE 36
IRQ_NOERRCODE 37
IRQ_NOERRCODE 38
IRQ_NOERRCODE 39
IRQ_NOERRCODE 40
IRQ_NOERRCODE 41
IRQ_NOERRCODE 42
IRQ_NOERRCODE 43
IRQ_NOERRCODE 44
IRQ_NOERRCODE 45
IRQ_NOERRCODE 46
IRQ_NOERRCODE 47
