#!/bin/bash
# run.sh — 使用 QEMU 在 Mac (M4/ARM64) 上运行 x86_64 内核
#
# 前提条件：
#   brew install x86_64-elf-gcc x86_64-elf-binutils qemu
#
# 用法：
#   ./scripts/run.sh           # 普通模式
#   ./scripts/run.sh -d        # 调试模式（等待 GDB 连接）
#   ./scripts/run.sh -m        # 监视模式（打开 QEMU monitor）

set -e

KERNEL="build/sros.bin"
QEMU=qemu-system-x86_64

# 检查内核文件是否存在
if [ ! -f "$KERNEL" ]; then
    echo "[!] 内核文件不存在，请先执行 make"
    echo "    make"
    exit 1
fi

# 基础 QEMU 参数
QEMU_ARGS=(
    -kernel      "$KERNEL"
    -m           128M
    -serial      stdio
    -no-reboot
    -no-shutdown
    -display     cocoa
)

# 不同运行模式
case "$1" in
    -d|--debug)
        # 调试模式：等待 GDB 连接 (localhost:1234)
        echo "[*] 调试模式启动中... (GDB 连接端口 :1234)"
        QEMU_ARGS+=(-s -S)
        ;;
    -m|--monitor)
        # 监视模式：同时打开 QEMU monitor
        QEMU_ARGS+=(-monitor stdio)
        # 串口输出改为文件
        for i in "${!QEMU_ARGS[@]}"; do
            if [ "${QEMU_ARGS[$i]}" = "stdio" ] && [ "${QEMU_ARGS[$i-1]}" = "-serial" ]; then
                QEMU_ARGS[$i]="file:serial.log"
            fi
        done
        echo "[*] 监视模式启动中... (串口日志: serial.log)"
        ;;
    *)
        echo "[*] 启动 SROS 内核..."
        echo "[*] 按 Ctrl+A 然后按 X 退出 QEMU"
        echo ""
        ;;
esac

exec "$QEMU" "${QEMU_ARGS[@]}"
