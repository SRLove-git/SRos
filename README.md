# SROS — 简易 x86 内核

一个运行在 32 位保护模式下的简约 x86 操作系统内核，使用交叉编译器构建，通过 QEMU 模拟运行。

## 功能特性

- **32 位保护模式** — 直接启动进入保护模式
- **分页内存管理** — 4KB 页表映射，支持物理页帧分配器
- **用户态支持** — Ring 3 用户模式跳转，TSS 提供内核栈切换
- **系统调用** — 通过 `int 0x80` 实现内核服务（读写字符）
- **GDT（全局描述符表）** — 段描述符设置，含用户代码/数据段和 TSS
- **IDT 和 IRQ** — 中断处理，支持 PIC 重映射和 PIT 定时器
- **VGA 文本模式** — 通过 `0xB8000` 帧缓冲进行屏幕输出，支持光标追踪
- **串口 I/O** — 通过 COM1 串口输出调试信息，支持 `printf` 风格格式化
- **PS/2 键盘** — 中断驱动的键盘输入，支持缓冲区

## 前置条件

在 macOS（M4/ARM64）上安装所需工具：

```bash
brew install x86_64-elf-gcc x86_64-elf-binutils qemu
```

## 构建与运行

```bash
make          # 构建内核
make run      # 构建并在 QEMU 中运行
make debug    # 构建并以调试模式启动（GDB 端口 :1234）
make monitor  # 构建并以监视模式启动
make clean    # 清理构建产物
```

## 项目结构

```
├── boot/             # 汇编入口和底层设置
│   ├── boot.s        # 32 位保护模式入口，设置栈，跳转到 kernel_main
│   ├── idt_asm.s     # IDT 中断入口包装
│   └── syscall_asm.s # 系统调用中断入口 (int 0x80)
├── drivers/          # 设备驱动
│   ├── keyboard.c    # PS/2 键盘驱动
│   ├── serial.c      # COM1 串口驱动（含 printf 格式化输出）
│   └── vga.c         # VGA 文本模式帧缓冲驱动（含光标追踪）
├── include/          # 头文件
│   ├── gdt.h, idt.h, irq.h, io.h
│   ├── keyboard.h, serial.h, vga.h
│   ├── paging.h      # 分页和物理页帧分配器
│   ├── syscall.h     # 系统调用定义
│   └── types.h       # 类型定义（u8, u16, u32 等）
├── kernel/           # 内核核心组件
│   ├── gdt.c         # GDT 初始化（含 TSS 和用户段）
│   ├── idt.c         # IDT 和 PIC 设置（含 int 0x80）
│   ├── irq.c         # IRQ 处理程序、PIT 定时器
│   ├── paging.c      # 分页初始化和缺页异常处理
│   ├── syscall.c     # 系统调用处理（SYS_READ/SYS_WRITE/SYS_GETCHAR）
│   └── kernel.c      # 内核入口点
├── scripts/
│   └── run.sh        # QEMU 启动脚本（普通/调试/监视模式）
├── linker.ld         # 链接脚本
├── Makefile          # 构建系统
└── README.md
```

## 系统调用

通过 `int 0x80` 中断提供以下用户态服务：

| 调用号 | 名称 | 参数 | 说明 |
|--------|------|------|------|
| 1 | `SYS_WRITE` | `ebx = 字符` | 输出一个字符到 VGA 屏幕 |
| 2 | `SYS_GETCHAR` | 无 | 阻塞读取一个键盘字符 |
| 3 | `SYS_READ` | `ebx = buf, ecx = len` | 非阻塞批量读取键盘输入 |

## 构建详情

内核使用 `x86_64-elf-` 交叉编译器工具链构建：

- **编译器**：`x86_64-elf-gcc`，参数 `-m32 -ffreestanding -nostdlib`
- **链接器**：`x86_64-elf-ld`，参数 `-m elf_i386`
- **输出**：由 QEMU 直接加载的扁平二进制文件（`build/sros.bin`）

## 调试

启动调试服务器：

```bash
make debug
```

然后在另一个终端中连接 GDB：

```bash
x86_64-elf-gdb build/sros.bin
(gdb) target remote localhost:1234
(gdb) continue
```

## 许可证

MIT
