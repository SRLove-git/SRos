# Makefile — SROS x86 Kernel Build System
#
# 使用 x86_64-elf- 交叉编译器（通过 Homebrew 安装）：
#   brew install x86_64-elf-gcc x86_64-elf-binutils
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

CFLAGS := \
    -ffreestanding \
    -fno-stack-protector \
    -fno-pic \
    -nostdlib \
    -m32 \
    -Wall -Wextra \
    -I include

LDFLAGS := \
    -m elf_i386 \
    -T linker.ld

# 源文件
C_SRCS := $(wildcard kernel/*.c drivers/*.c)
ASM_SRCS := $(wildcard boot/*.s)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS += $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))

.PHONY: all run debug monitor clean

all: $(BUILD_DIR)/sros.bin

# 链接
$(BUILD_DIR)/sros.bin: $(OBJS) linker.ld
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 编译 C
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

# 汇编 .s
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(@D)
	$(AS) --32 -o $@ $<

# 运行
run: all
	@./scripts/run.sh

# 调试模式
debug: all
	@./scripts/run.sh -d

# 监视模式
monitor: all
	@./scripts/run.sh -m

# 清理
clean:
	rm -rf $(BUILD_DIR)
	rm -f serial.log

# 显示每个源文件的大小
size: $(BUILD_DIR)/sros.bin
	$(CROSS)objdump -h $(BUILD_DIR)/sros.bin
