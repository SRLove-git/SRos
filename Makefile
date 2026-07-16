# Makefile — SROS x86 Kernel Build System
#
# macOS 使用 x86_64-elf- 交叉编译器（通过 Homebrew 安装）：
#   brew install x86_64-elf-gcc x86_64-elf-binutils
#
# Linux 使用包管理器安装（详见 README.md）：
#   sudo apt install gcc-x86_64-elf qemu-system-x86
#
# Windows（详见 README.md）：
#   1. 下载 https://github.com/lordmilko/i686-elf-tools/releases/tag/15.2.0
#      中的 x86_64-elf-tools-windows.zip，解压并添加到 PATH
#   2. scoop install qemu
#
# 常用命令：
#   make            构建内核
#   make run        构建并运行
#   make clean      清理构建产物
#   make debug      构建并以调试模式启动
#   make monitor    构建并以监视模式启动

CROSS := x86_64-elf-
CC    := $(CROSS)gcc
LD    := $(CROSS)ld
AS    := $(CROSS)as

BUILD_DIR := build

# 用户程序编译标志
USER_CFLAGS := \
    -ffreestanding \
    -fno-stack-protector \
    -fno-pic \
    -nostdlib \
    -m32 \
    -Wall -Wextra \
    -I include

# 用户程序链接脚本
USER_LD_SCRIPT := user/user.ld

# 用户程序 ELF 文件
USER_PROGRAMS := $(BUILD_DIR)/user/hello.elf

# 磁盘镜像
DISK_IMG := $(BUILD_DIR)/disk_copy.img

CFLAGS := \
    -ffreestanding \
    -fno-stack-protector \
    -fno-pic \
    -nostdlib \
    -m32 \
    -mno-sse -mno-mmx -mno-80387 \
    -Wall -Wextra \
    -I include \
	-I user \
	-I fs

LDFLAGS := \
    -m elf_i386 \
    -T linker.ld

# 源文件（排除用户程序，用户程序单独编译为独立 ELF）
C_SRCS := $(filter-out user/hello.c, $(wildcard kernel/*.c drivers/*.c user/*.c fs/*.c))
ASM_SRCS := $(wildcard boot/*.s)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS += $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))

.PHONY: all run debug monitor clean

all: $(BUILD_DIR)/sros.bin $(USER_PROGRAMS)

# 编译用户程序（独立 ELF，不链接到内核）
$(BUILD_DIR)/%.elf: %.c $(USER_LD_SCRIPT)
	@$(call make_dir,$(@D))
	$(CC) $(USER_CFLAGS) -Wl,-m -Wl,elf_i386 -T $(USER_LD_SCRIPT) -o $@ $<

# 创建磁盘镜像
ifeq ($(OS),Windows_NT)
$(DISK_IMG): $(BUILD_DIR)/sros.bin
	@echo "[*] Creating disk image: $(DISK_IMG)"
	@powershell -Command "$$f = [System.IO.File]::Create('$(subst /,\,$(DISK_IMG))'); $$f.SetLength(4*1024*1024); $$f.Close()"
else
$(DISK_IMG): $(BUILD_DIR)/sros.bin
	@echo "[*] Creating disk image: $(DISK_IMG)"
	@dd if=/dev/zero of=$(DISK_IMG) bs=512 count=8192 2>/dev/null
	@if [ -f tools/inject_file.py ]; then \
		echo "[*] Injecting user programs..."; \
		python3 tools/inject_file.py $(DISK_IMG) $(BUILD_DIR)/user/hello.elf hello.elf --format; \
	else \
		echo "[*] tools/inject_file.py not found, skipping injection."; \
	fi
endif

# Windows 系统命令适配
ifeq ($(OS),Windows_NT)
MKDIR := if not exist
RMDIR := rmdir /s /q
RMFILE := del /f /q
else
MKDIR := mkdir -p
RMDIR := rm -rf
RMFILE := rm -f
endif

# 创建目录（兼容 Windows 和 Unix）
ifeq ($(OS),Windows_NT)
define make_dir
	$(MKDIR) $(1) mkdir $(1)
endef
else
define make_dir
	mkdir -p $(1)
endef
endif

# 链接
$(BUILD_DIR)/sros.bin: $(OBJS) linker.ld
	@$(call make_dir,$(@D))
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 编译 C
$(BUILD_DIR)/%.o: %.c
	@$(call make_dir,$(@D))
	$(CC) $(CFLAGS) -c -o $@ $<

# 汇编 .s
$(BUILD_DIR)/%.o: %.s
	@$(call make_dir,$(@D))
	$(AS) --32 -o $@ $<

# 检测操作系统
ifeq ($(OS),Windows_NT)
RUN_SCRIPT := scripts\\run.bat
else
RUN_SCRIPT := ./scripts/run.sh
endif

# 运行
run: all $(DISK_IMG)
	@$(RUN_SCRIPT)

# 调试模式
debug: all $(DISK_IMG)
	@$(RUN_SCRIPT) -d

# 监视模式
monitor: all $(DISK_IMG)
	@$(RUN_SCRIPT) -m

# 清理
clean:
	-$(RMDIR) $(BUILD_DIR)
	-$(RMFILE) serial.log

# 显示每个源文件的大小
size: $(BUILD_DIR)/sros.bin
	$(CROSS)objdump -h $(BUILD_DIR)/sros.bin
