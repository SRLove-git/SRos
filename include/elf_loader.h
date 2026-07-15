#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "types.h"
#include "idt.h"

/* 加载 ELF 文件并跳转执行
 * path: SFS 中的 ELF 文件路径
 * regs: 当前系统调用的寄存器状态（将被修改以跳转到用户程序）
 * 
 * 返回: 成功则不返回（通过 iret 跳转到用户程序），失败返回 -1
 */
int elf_load_and_exec(const char *path, registers_t *regs);

#endif /* ELF_LOADER_H */
