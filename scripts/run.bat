@echo off
REM run.bat — 使用 QEMU 在 Windows 上运行 x86_64 内核
REM
REM 前提条件（使用 Scoop 安装）：
REM   scoop install gcc-x86_64-elf qemu
REM
REM 或者使用 MSYS2：
REM   pacman -S mingw-w64-x86_64-x86_64-elf-gcc mingw-w64-x86_64-qemu
REM
REM 用法：
REM   scripts\run.bat             普通模式
REM   scripts\run.bat -d          调试模式（等待 GDB 连接）
REM   scripts\run.bat -m          监视模式（打开 QEMU monitor）

setlocal enabledelayedexpansion

set KERNEL=build\sros.bin
set QEMU=qemu-system-x86_64

REM 检查内核文件是否存在
if not exist "%KERNEL%" (
    echo [!] 内核文件不存在，请先执行 make
    echo     make
    exit /b 1
)

REM 基础 QEMU 参数
set QEMU_ARGS=-kernel "%KERNEL%" -m 128M -serial stdio -no-reboot -no-shutdown

REM 不同运行模式
if /i "%1"=="-d" (
    echo [*] 调试模式启动中... (GDB 连接端口 :1234)
    set QEMU_ARGS=%QEMU_ARGS% -s -S
) else if /i "%1"=="--debug" (
    echo [*] 调试模式启动中... (GDB 连接端口 :1234)
    set QEMU_ARGS=%QEMU_ARGS% -s -S
) else if /i "%1"=="-m" (
    echo [*] 监视模式启动中... (串口日志: serial.log)
    REM 将串口输出改为文件
    set "QEMU_ARGS=!QEMU_ARGS:-serial stdio=-serial file:serial.log!"
    set QEMU_ARGS=%QEMU_ARGS% -monitor stdio
) else if /i "%1"=="--monitor" (
    echo [*] 监视模式启动中... (串口日志: serial.log)
    REM 将串口输出改为文件
    set "QEMU_ARGS=!QEMU_ARGS:-serial stdio=-serial file:serial.log!"
    set QEMU_ARGS=%QEMU_ARGS% -monitor stdio
) else (
    echo [*] 启动 SROS 内核...
    echo [*] 按 Ctrl+Alt 然后按 G 释放鼠标，关闭窗口退出 QEMU
    echo/
)

echo %QEMU% %QEMU_ARGS%
%QEMU% %QEMU_ARGS%
