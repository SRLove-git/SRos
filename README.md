<div align="center">

# 🖥️ SROS

**Simple x86 Operating System Kernel**

*一个运行在 32 位保护模式下的简约 x86 操作系统内核*

<br>

![Language](https://img.shields.io/badge/language-C%20%2B%20Assembly-blue)
![Arch](https://img.shields.io/badge/arch-x86_32-orange)
![Build](https://img.shields.io/badge/build-x86__64--elf--gcc-green)
![Emulator](https://img.shields.io/badge/emulator-QEMU-red)
![License](https://img.shields.io/badge/license-MIT-lightgrey)
![Status](https://img.shields.io/badge/status-active-brightgreen)

<br>

[✨ 特性](#-特性) • [📦 前置条件](#-前置条件) • [🚀 快速开始](#-快速开始) • [📁 项目结构](#-项目结构) • [⚙️ 系统调用](#️-系统调用) • [🔧 调试](#-调试)

</div>

---

## ✨ 特性

<table>
<tr>
<td width="50%">

**核心** 🧠
- 32 位保护模式启动
- GDT 段描述符（含用户段 & TSS）
- IDT 中断处理 & PIC 重映射
- PIT 定时器 & 内核态中断
- **分页内存管理**（4KB 页表）

</td>
<td width="50%">

**用户态** 👤
- Ring 3 用户模式跳转
- TSS 内核栈自动切换
- **`int 0x80` 系统调用**
  - `SYS_WRITE` — 输出字符
  - `SYS_GETCHAR` — 阻塞读键盘
  - `SYS_READ` — 批量读取

</td>
</tr>
<tr>
<td width="50%">

**驱动** 🛠️
- VGA 文本模式（`0xB8000`）
- 硬件光标追踪 & 退格键
- 串口 COM1 调试输出（`printf` 风格）
- PS/2 键盘中断驱动 + 缓冲区

</td>
<td width="50%">

**调试** 🔍
- GDB 远程调试（`:1234`）
- QEMU Monitor 模式
- 串口日志输出
- 异常处理 & 内核恐慌

</td>
</tr>
</table>

---

## 📦 前置条件

在 macOS（ARM64）上安装所需工具：

```bash
brew install x86_64-elf-gcc x86_64-elf-binutils qemu
```

---

## 🚀 快速开始

```bash
# 构建内核
make

# 构建并运行
make run

# 调试模式（等待 GDB 连接）
make debug

# 监视模式（打开 QEMU monitor）
make monitor

# 清理构建产物
make clean
```

> 在 QEMU 中按 **Ctrl+A** 再按 **X** 可退出。

---

## 📁 项目结构

```
SROS/
├── boot/                    # 🏁 汇编入口
│   ├── boot.s               #     保护模式入口 → kernel_main
│   ├── idt_asm.s            #     IDT 中断包装
│   └── syscall_asm.s        #     int 0x80 系统调用入口
│
├── kernel/                  # 🧠 内核核心
│   ├── kernel.c             #     入口点
│   ├── gdt.c                #     GDT + TSS + 用户段
│   ├── idt.c                #     IDT + int 0x80 + 缺页处理
│   ├── irq.c                #     IRQ + PIT 定时器
│   ├── paging.c             #     分页 & 物理页帧分配
│   └── syscall.c            #     系统调用处理
│
├── drivers/                 # 🛠️ 设备驱动
│   ├── vga.c                #     VGA 文本模式 + 光标
│   ├── keyboard.c           #     PS/2 键盘 + 缓冲区
│   └── serial.c             #     COM1 串口 + printf
│
├── include/                 # 📋 头文件
│   ├── types.h              #     u8/u16/u32 定义
│   ├── io.h                 #     inb/outb 端口 I/O
│   ├── gdt.h / idt.h / irq.h / paging.h / syscall.h
│   └── keyboard.h / serial.h / vga.h
│
├── scripts/
│   └── run.sh               #     QEMU 启动脚本
├── linker.ld                # 🔗 链接脚本
├── Makefile                 # 🏗️ 构建系统
└── README.md
```

---

## ⚙️ 系统调用

通过 `int 0x80` 中断向用户态提供服务：

| 调用号 | 名称 | 参数 | 说明 |
|:------:|:----:|:----|:-----|
| 1 | `SYS_WRITE` | `ebx = 字符` | 输出一个字符到屏幕 |
| 2 | `SYS_GETCHAR` | — | 阻塞等待键盘输入 |
| 3 | `SYS_READ` | `ebx = buf`, `ecx = len` | 非阻塞批量读取 |

```c
// 用户态示例：回显键盘输入
void user_function(void)
{
    while (1) {
        char c;
        __asm__ volatile("mov $2, %%eax; int $0x80" : "=a"(c));   // getchar
        __asm__ volatile("mov $1, %%eax; mov %0, %%ebx; int $0x80" :: "r"((u32)c));  // write
    }
}
```

---

## 🔧 调试

```bash
# 终端 1：启动调试服务器
make debug

# 终端 2：连接 GDB
x86_64-elf-gdb build/sros.bin
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

---

## 🏗️ 构建详情

| 组件 | 工具 | 参数 |
|:-----|:-----|:-----|
| 编译器 | `x86_64-elf-gcc` | `-m32 -ffreestanding -nostdlib` |
| 链接器 | `x86_64-elf-ld` | `-m elf_i386 -T linker.ld` |
| 输出 | `build/sros.bin` | QEMU 直接加载的扁平二进制 |

---

<div align="center">

**MIT License** © 2026 SROS

</div>
