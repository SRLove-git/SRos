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
- **分页内存管理**（4KB 页表 + 自映射）
- **物理页帧分配器**（PFA）
- **堆内存分配器**（kmalloc）

</td>
<td width="50%">

**进程管理** 📋
- **抢占式多任务调度**（时间片轮转）
- **`fork` 系统调用** — 创建子进程
- **`exit` 系统调用** — 进程退出
- **`wait` 系统调用** — 等待子进程
- **`exec` 系统调用** — ELF 程序加载
- **进程状态**：就绪/运行/阻塞/僵尸

</td>
</tr>
<tr>
<td width="50%">

**进程间通信 (IPC)** 🔄
- **计数信号量** — 创建、等待 (P)、释放 (V)、销毁
- **消息队列** — 创建、发送、接收、销毁
- **阻塞式 IPC** — 资源不可用时自动阻塞等待
- **`sem_test` / `msg_test` Shell 测试命令**
- **关中断保护** — `cli`/`sti` 临界区保护

</td>
</tr>
<tr>
<td width="50%">

**用户态** 👤
- Ring 3 用户模式跳转
- TSS 内核栈自动切换
- **`int 0x80` 系统调用**（25 个调用号）
- **交互式 Shell**（echo / clear / help / ls / cat / touch / write / exec / fork_test）

</td>
<td width="50%">

**文件系统** 📁
- **SFS** — 简单文件系统
- 文件创建/打开/读写/关闭
- 磁盘块管理与目录项
- 启动时自动挂载

</td>
</tr>
<tr>
<td width="50%">

**驱动** 🛠️
- VGA 文本模式（`0xB8000`）
- **VBE 帧缓冲** — 1024×768 @ 32bpp 线性帧缓冲
- **PS/2 鼠标驱动** — 轮询模式 + 环形缓冲区
- **软件鼠标光标** — 16×16 箭头精灵，save/restore 合成
- **窗口管理器** — Z 序多窗口叠加、焦点、关闭按钮
- 串口 COM1 调试输出（`printf` 风格）
- PS/2 键盘中断驱动 + 缓冲区

</td>
<td width="50%">

**调试** 🔍
- GDB 远程调试（`:1234`）
- QEMU Monitor 模式
- 串口日志输出
- 异常处理 & 内核恐慌
- ELF 可执行程序加载

</td>
</tr>
</table>

---

## 📦 前置条件

### macOS（ARM64）

```bash
brew install x86_64-elf-gcc x86_64-elf-binutils qemu
```

### Linux

**Ubuntu / Debian**

```bash
sudo apt install gcc-x86_64-elf qemu-system-x86
```

**Fedora**

```bash
sudo dnf install gcc-x86_64-elf qemu-system-x86
```

**Arch Linux / Manjaro**

```bash
sudo pacman -S qemu-system-x86
# x86_64-elf-gcc 可通过 AUR（如 yay）安装：
# yay -S x86_64-elf-gcc
```

### Windows

方式一：下载预编译工具链（推荐）

1. 下载 [x86_64-elf-tools-windows.zip](https://github.com/lordmilko/i686-elf-tools/releases/download/15.2.0/x86_64-elf-tools-windows.zip)
2. 解压到 `C:\x86_64-elf-tools`
3. 将 `C:\x86_64-elf-tools\bin` 添加到系统 PATH：
   ```powershell
   [Environment]::SetEnvironmentVariable("Path", "C:\x86_64-elf-tools\bin;" + [Environment]::GetEnvironmentVariable("Path", "User"), "User")
   ```
4. 安装 QEMU（通过 Scoop）：
   ```powershell
   scoop install qemu
   ```

方式二：使用 **MSYS2**

```bash
pacman -S mingw-w64-x86_64-x86_64-elf-gcc mingw-w64-x86_64-qemu
```

> 安装完成后，在 PowerShell 或 MSYS2 终端中执行 `make` 即可构建。

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

> 在 QEMU 中按 **Ctrl+A** 再按 **X** 可退出（macOS/Linux）。
> Windows 下直接关闭 QEMU 窗口即可退出。

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
│   ├── paging.c             #     分页 & 物理页帧分配器
│   ├── scheduler.c          #     进程调度器（RR + fork/exit/wait）
│   ├── syscall.c            #     系统调用处理（25 个调用号）
│   ├── ipc.c                #     信号量 & 消息队列（IPC）
│   ├── elf_loader.c         #     ELF 可执行程序加载器
│   ├── kmalloc.c            #     堆内存分配器
│   └── string.c             #     字符串工具函数
│
├── drivers/                 # 🛠️ 设备驱动
│   ├── vga.c                #     VGA 文本模式 + 光标
│   ├── framebuffer.c        #     VBE 线性帧缓冲（1024×768 @ 32bpp）
│   ├── mouse.c              #     PS/2 鼠标驱动（轮询 + 环形缓冲）
│   ├── cursor.c             #     软件鼠标光标（save/restore 合成）
│   ├── window.c             #     窗口管理器（Z 序、焦点、合成）
│   ├── keyboard.c           #     PS/2 键盘 + 缓冲区
│   └── serial.c             #     COM1 串口 + printf
│
├── include/                 # 📋 头文件
│   ├── types.h              #     u8/u16/u32 定义
│   ├── io.h                 #     inb/outb 端口 I/O
│   ├── string.h             #     字符串工具函数声明
│   ├── framebuffer.h        #     VBE 帧缓冲接口
│   ├── mouse.h              #     PS/2 鼠标事件 & 驱动接口
│   ├── cursor.h             #     软件光标合成辅助
│   ├── window.h             #     窗口管理器 API & 结构体
│   ├── gdt.h / idt.h / irq.h / paging.h / syscall.h / ipc.h
│   ├── scheduler.h / elf_loader.h / kmalloc.h
│   └── keyboard.h / serial.h / vga.h
│
├── user/                    # 👤 用户态程序
│   ├── shell.c              #     交互式命令行
│   └── shell.h              #     Shell 接口声明
│
├── fs/                      # 📁 文件系统
│   ├── ramdisk.c            #     内存磁盘驱动
│   └── sfs.c                #     SFS 简单文件系统
│
├── scripts/
│   ├── run.sh               #     QEMU 启动脚本（macOS/Linux）
│   └── run.bat              #     QEMU 启动脚本（Windows）
├── linker.ld                # 🔗 链接脚本
├── Makefile                 # 🏗️ 构建系统
└── README.md
```

---

## ⚙️ 系统调用

通过 `int 0x80` 中断向用户态提供服务，根据 `ebx`、`ecx`、`edx` 传递参数，返回值通过 `eax` 返回。当前共 25 个系统调用：

| 调用号 | 名称 | 参数 | 说明 |
|:------:|:----:|:----|:-----|
| 0 | `SYS_READ` | `ebx = buf`, `ecx = len` | 非阻塞批量读取键盘 |
| 1 | `SYS_WRITE` | `ebx = 字符` | 输出一个字符到屏幕 |
| 2 | `SYS_GETCHAR` | — | 阻塞等待键盘输入 |
| 3 | `SYS_CLEAR` | — | 清屏 |
| 4 | `SYS_FS_OPEN` | `ebx = path` | 打开文件 |
| 5 | `SYS_FS_READ` | `ebx = fd`, `ecx = buf`, `edx = count` | 读文件 |
| 6 | `SYS_FS_WRITE` | `ebx = fd`, `ecx = buf`, `edx = count` | 写文件 |
| 7 | `SYS_FS_CLOSE` | `ebx = fd` | 关闭文件 |
| 8 | `SYS_FS_CREATE` | `ebx = name` | 创建文件 |
| 9 | `SYS_FS_LS` | `ebx = buf`, `ecx = size` | 列出根目录 |
| 10 | `SYS_KMALLOC` | `ebx = size` | 分配堆内存 |
| 11 | `SYS_KFREE` | `ebx = ptr` | 释放堆内存 |
| 12 | `SYS_KMALLOC_DUMP` | — | 打印堆状态 |
| 13 | `SYS_EXEC` | `ebx = path` | 加载并执行 ELF 程序 |
| 14 | `SYS_EXIT` | `ebx = exit_code` | 退出当前进程 |
| 15 | `SYS_FORK` | — | 创建子进程 |
| 16 | `SYS_WAIT` | `ebx = pid` | 等待子进程退出 |
| 17 | `SYS_SEM_INIT` | `ebx = name`, `ecx = val` | 创建信号量 |
| 18 | `SYS_SEM_WAIT` | `ebx = sem_id` | 信号量 P 操作 |
| 19 | `SYS_SEM_POST` | `ebx = sem_id` | 信号量 V 操作 |
| 20 | `SYS_SEM_DESTROY` | `ebx = sem_id` | 销毁信号量 |
| 21 | `SYS_MSGGET` | — | 创建消息队列 |
| 22 | `SYS_MSGSND` | `ebx = mq_id`, `ecx = buf`, `edx = len` | 发送消息 |
| 23 | `SYS_MSGRCV` | `ebx = mq_id`, `ecx = buf`, `edx = &len` | 接收消息 |
| 24 | `SYS_MSGCTL` | `ebx = mq_id` | 销毁消息队列 |

```c
// 用户态示例：Shell 交互命令行（支持命令历史、行编辑）
void shell_main(void)
{
    char line[256];
    int  pos = 0;
    puts("SRos> ");
    while (1) {
        char c = getchar();
        if (c == '\n') {
            putchar('\n');
            line[pos] = '\0';
            history_add(line);
            execute(line);            // 解析并执行命令
            pos = 0;
            history_index = -1;
            puts("SRos> ");
        }
        else if (c == '\b') {
            if (pos > 0) {
                pos--;
                // 行内删除 + 光标重绘
            }
        }
        // 支持方向键 ↑↓ 浏览历史、←→ 移动光标
        else if (pos < 255) {
            line[pos++] = c;
            putchar(c);
        }
    }
}
```

支持命令：`echo`、`clear`、`help`、`ls`、`cat`、`touch`、`write`、`kmalloc`、`kfree`、`kmdump`、`exec`、`fork_test`、`sem_test`、`msg_test`

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

> Windows 下同样支持 GDB 调试（Scoop/MSYS2 均已包含 `x86_64-elf-gdb`）。

---

## 📋 待办

- [x] 串口输出 — COM1 调试输出（printf 风格）
- [x] GDT — 全局描述符表（含用户段 & TSS）
- [x] IDT — 中断描述符表 & PIC 重映射
- [x] IRQ — 硬件中断处理（键盘/定时器）
- [x] PIT — 可编程间隔定时器
- [x] VGA 文本模式 — 0xB8000 显示 + 硬件光标
- [x] VBE 帧缓冲 — 1024×768 @ 32bpp 线性帧缓冲
- [x] PS/2 鼠标驱动 — 轮询模式 + 环形缓冲区
- [x] 软件鼠标光标 — 16×16 箭头精灵，save/restore 合成
- [x] 窗口管理器 — Z 序多窗口叠加、焦点、关闭按钮、合成
- [x] PS/2 键盘驱动 — 中断驱动 + 环形缓冲区
- [x] 分页 — 4KB 页表 + 物理页帧分配器
- [x] 用户态 — Ring 3 跳转 + TSS 内核栈切换
- [x] 系统调用 — int 0x80（SYS_WRITE / SYS_GETCHAR / SYS_READ / SYS_CLEAR）
- [x] Shell — 交互式命令行（echo、clear、help）
- [x] 字符串库 — memset、strcmp 等工具函数
- [x] 调试支持 — GDB 远程调试 + QEMU Monitor
- [x] 异常处理 — 缺页异常、通用保护故障等
- [x] 多平台构建 — macOS / Linux / Windows
- [x] 堆内存分配器 (kmalloc) — 内核动态内存分配
- [x] 文件系统 — 磁盘读写与文件管理
- [x] 进程管理 — 抢占式多任务调度（fork/exit/wait）
- [x] ELF 程序加载器 — exec 系统调用
- [x] 进程间通信 (IPC) — 计数信号量 & 消息队列
- [ ] 更多...

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
