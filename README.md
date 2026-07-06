# SROS - Simple x86 Operating System Kernel

SROS 是一个从零开始的简单 x86 操作系统内核，运行在 32 位保护模式下。项目旨在通过实践的方式学习操作系统底层原理，包括引导、串口通信、中断处理、内存管理等核心概念。

## 项目结构

```
sros/
├── boot/           # 引导程序
│   └── boot.s      # Multiboot 规范的引导入口
├── kernel/         # 内核核心
│   └── kernel.c    # 内核入口及主循环
├── drivers/        # 设备驱动
│   └── serial.c    # 串口驱动 (COM1)
├── include/        # 头文件
│   ├── types.h     # 基础类型定义 (u8, u16, u32 等)
│   ├── io.h        # 端口 I/O 内联汇编 (in/out 指令)
│   └── serial.h    # 串口驱动接口
├── build/          # 构建产物目录
├── scripts/
│   └── run.sh      # QEMU 运行脚本
├── Makefile        # 构建系统
├── linker.ld       # 链接脚本
└── README.md       # 项目文档
```

## 环境要求

- macOS (Apple Silicon)
- 交叉编译器: `x86_64-elf-gcc` + `x86_64-elf-binutils`
- QEMU (x86_64 模拟器)

### 安装依赖

```bash
brew install x86_64-elf-gcc x86_64-elf-binutils qemu
```

## 快速开始

### 构建内核

```bash
make
```

### 运行内核

```bash
make run
```

### 其他命令

| 命令 | 说明 |
|------|------|
| `make` | 构建内核镜像 |
| `make run` | 构建并运行内核 |
| `make debug` | 以调试模式启动 (等待 GDB 连接 :1234) |
| `make monitor` | 以监视模式启动 (打开 QEMU Monitor) |
| `make clean` | 清理构建产物 |
| `make size` | 查看各段大小 |

## 运行输出

启动后通过串口输出如下信息：

```
========================================
  SROS - Simple x86 Kernel
  内核已成功启动！
========================================

>> 输入 'help' 查看可用命令

>
```

## 运行模式

### 普通模式 (`make run`)
串口输出直接显示在终端，按 `Ctrl+A` 然后按 `X` 退出 QEMU。

### 调试模式 (`make debug`)
启动 QEMU 并等待 GDB 连接 (默认端口 :1234)：

```bash
# 终端 1：启动调试模式
make debug

# 终端 2：连接 GDB
x86_64-elf-gdb build/sros.bin
(gdb) target remote :1234
(gdb) continue
```

### 监视模式 (`make monitor`)
同时打开 QEMU Monitor，串口日志输出到 `serial.log` 文件。

## 技术细节

### 引导流程

1. QEMU 加载 `build/sros.bin` 到内存 `0x100000` (1MB)
2. GRUB Multiboot 协议引导，跳转到 `_start` 入口
3. `boot.s` 设置栈指针 (`stack_top`)
4. 调用 `kernel_main` 进入 C 内核

### 串口驱动

- 使用 COM1 端口 (`0x3F8`)
- 波特率: 115200 (除数为 1)
- 数据位: 8, 停止位: 1, 无校验
- 支持输出: 字符、字符串、十六进制数

### 链接布局

| 段 | 起始地址 | 说明 |
|----|---------|------|
| `.text` | 1MB | Multiboot 头 + 代码段 |
| `.rodata` | 对齐 4K | 只读数据 |
| `.data` | 对齐 4K | 读写数据 |
| `.bss` | 对齐 4K | 未初始化数据 (含 16KB 栈) |

## 学习路线

当前内核已实现的基础功能：
- [x] Multiboot 兼容引导
- [x] 保护模式入口
- [x] 串口调试输出

待实现的学习任务（可自行尝试）：
- [ ] 初始化 GDT/IDT
- [ ] 注册 IRQ 中断处理
- [ ] VGA 文本模式输出 (0xB8000)
- [ ] A20 地址线启用
- [ ] 分页机制
- [ ] 键盘驱动
- [ ] 简单 Shell
- [ ] 内存管理

## License

MIT
