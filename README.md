# SROS — 简易 x86 内核

一个运行在 32 位保护模式下的简约 x86 操作系统内核，使用交叉编译器构建，通过 QEMU 模拟运行。

## 功能特性

- **32 位保护模式** — 直接启动进入保护模式
- **GDT（全局描述符表）** — 段描述符设置
- **IDT 和 IRQ** — 中断处理，支持 PIC 重映射和 PIT 定时器
- **VGA 文本模式** — 通过 `0xB8000` 帧缓冲进行屏幕输出
- **串口 I/O** — 通过 COM1 串口输出调试信息
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
├── boot/           # 汇编入口和底层设置
│   ├── boot.s      # 32 位保护模式入口，设置栈，跳转到 kernel_main
│   └── idt_asm.s   # IDT 中断入口包装
├── drivers/        # 设备驱动
│   ├── keyboard.c  # PS/2 键盘驱动
│   ├── serial.c    # COM1 串口驱动
│   └── vga.c       # VGA 文本模式帧缓冲驱动
├── include/        # 头文件
│   ├── gdt.h, idt.h, irq.h, io.h
│   ├── keyboard.h, serial.h, vga.h
│   └── types.h     # 类型定义（u8, u16, u32 等）
├── kernel/         # 内核核心组件
│   ├── gdt.c       # GDT 初始化
│   ├── idt.c       # IDT 和 PIC 设置
│   ├── irq.c       # IRQ 处理程序、PIT 定时器
│   └── kernel.c    # 内核入口点
├── scripts/
│   └── run.sh      # QEMU 启动脚本（普通/调试/监视模式）
├── linker.ld       # 链接脚本
├── Makefile        # 构建系统
└── README.md
```

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
