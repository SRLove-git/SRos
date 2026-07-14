@echo off
REM run.bat - Use QEMU on Windows to run x86_64 kernel
REM
REM Prerequisites:
REM   1. Download x86_64-elf-tools from:
REM      https://github.com/lordmilko/i686-elf-tools/releases/tag/15.2.0
REM      and add bin/ to PATH
REM   2. Install QEMU (via Scoop): scoop install qemu
REM
REM Usage:
REM   scripts\run.bat             Normal mode
REM   scripts\run.bat -d          Debug mode (wait for GDB connection)
REM   scripts\run.bat -m          Monitor mode (open QEMU monitor)

setlocal enabledelayedexpansion

set KERNEL=build\sros.bin
set QEMU=qemu-system-x86_64

REM Check if kernel file exists
if not exist "%KERNEL%" (
    echo [ERROR] Kernel binary not found, run 'make' first.
    exit /b 1
)

REM Base QEMU arguments
set QEMU_ARGS=-kernel "%KERNEL%" -m 128M -serial stdio -no-reboot -no-shutdown

REM Parse mode argument
if /i "%1"=="-d" goto debug_mode
if /i "%1"=="--debug" goto debug_mode
if /i "%1"=="-m" goto monitor_mode
if /i "%1"=="--monitor" goto monitor_mode
goto normal_mode

:debug_mode
echo [*] Debug mode starting... (GDB port :1234)
set QEMU_ARGS=%QEMU_ARGS% -s -S
goto run_qemu

:monitor_mode
echo [*] Monitor mode starting... (serial log: serial.log)
set "QEMU_ARGS=!QEMU_ARGS:-serial stdio=-serial file:serial.log!"
set QEMU_ARGS=%QEMU_ARGS% -monitor stdio
goto run_qemu

:normal_mode
echo [*] Starting SROS kernel...
echo [*] Press Ctrl+Alt then G to release mouse, close window to exit QEMU
echo/
goto run_qemu

:run_qemu
echo %QEMU% %QEMU_ARGS%
%QEMU% %QEMU_ARGS%
