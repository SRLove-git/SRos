#include "elf_loader.h"
#include "elf.h"
#include "paging.h"
#include "sfs.h"
#include "ramdisk.h"
#include "serial.h"
#include "string.h"

/* 用户栈配置 */
#define USER_STACK_VADDR    0xE0000000  /* 用户栈顶 */
#define USER_STACK_PAGES    4           /* 16KB 用户栈 */

/*
 * 读取文件 ino 从 offset 处读取 size 字节到 buf
 * 返回实际读取字节数，失败返回 -1
 */
static int read_file_at(u32 ino, u8 *buf, u32 size, u32 offset)
{
    struct sfs_inode inode;
    if (sfs_read_inode(ino, &inode) < 0) return -1;
    return sfs_read_file(&inode, buf, size, offset);
}

/*
 * 将文件 ino 的 [offset, offset+size) 数据逐页加载到内存
 * 每页映射到 virt_addr + current_offset 处（用户可访问）
 * 返回 0 成功，-1 失败
 */
static int load_segment(u32 ino, u32 virt_addr, u32 offset, u32 filesz, u32 memsz)
{
    u8 page_buf[4096];
    u32 curr_vaddr = virt_addr;

    /* 加载文件数据部分（filesz） */
    u32 file_remain = filesz;
    u32 file_offset = offset;

    while (file_remain > 0) {
        u32 page_offset = curr_vaddr & 0xFFF;
        u32 to_read = 4096 - page_offset;
        if (to_read > file_remain) to_read = file_remain;

        /* 分配物理页并映射 */
        paging_alloc_page(curr_vaddr & 0xFFFFF000);

        /* 从文件读取数据到临时缓冲区 */
        memset(page_buf, 0, 4096);
        int n = read_file_at(ino, page_buf, to_read, file_offset);
        if (n < 0) return -1;

        /* 将数据复制到映射的页面中 */
        /* 由于新映射的页面我们还没有直接的虚拟地址访问权限，
         * 但内核的 identity mapping 覆盖了 0-4MB，新分配的
         * 物理页可能不在其中。使用临时映射技巧。
         *
         * 利用页目录自映射访问用户页面：
         * 用户地址 curr_vaddr & ~0xFFF 通过自映射写入
         */
        u32 page_vaddr = curr_vaddr & 0xFFFFF000;
        u8 *page_ptr = (u8 *)page_vaddr;
        for (u32 i = 0; i < to_read; i++)
            page_ptr[page_offset + i] = page_buf[i];

        file_remain -= to_read;
        file_offset += to_read;
        curr_vaddr += to_read;
    }

    /* .bss 部分清零（memsz - filesz） */
    u32 bss_remain = memsz - filesz;
    u32 bss_start = virt_addr + filesz;
    curr_vaddr = bss_start;

    while (bss_remain > 0) {
        u32 page_offset = curr_vaddr & 0xFFF;
        /* 检查当前页是否已经映射 */
        u32 *page_dir;
        __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir));
        u32 pd_idx = curr_vaddr >> 22;
        u32 pt_idx = (curr_vaddr >> 12) & 0x3FF;

        if (!(page_dir[pd_idx] & 0x01) ||
            !(((u32 *)(0xFFC00000 + pd_idx * 4096))[pt_idx] & 0x01)) {
            /* 页未映射，分配新页 */
            paging_alloc_page(curr_vaddr & 0xFFFFF000);
        }

        u32 page_vaddr = curr_vaddr & 0xFFFFF000;
        u8 *page_ptr = (u8 *)page_vaddr;
        u32 to_zero = 4096 - page_offset;
        if (to_zero > bss_remain) to_zero = bss_remain;
        for (u32 i = 0; i < to_zero; i++)
            page_ptr[page_offset + i] = 0;

        bss_remain -= to_zero;
        curr_vaddr += to_zero;
    }

    return 0;
}

/*
 * 分配并映射用户栈
 */
static int setup_user_stack(void)
{
    for (u32 i = 0; i < USER_STACK_PAGES; i++) {
        u32 stack_page = USER_STACK_VADDR - (i + 1) * 0x1000;
        paging_alloc_page(stack_page);
    }
    return 0;
}

int elf_load_and_exec(const char *path, registers_t *regs)
{
    serial_printf(SERIAL_COM1, "[elf] Loading: %s\n", path);

    /* 1. 查找文件 */
    const char *name = path;
    while (*name == '/') name++;
    if (*name == '\0') return -1;

    int ino = sfs_lookup(ROOT_INO, name);
    if (ino < 0) {
        serial_printf(SERIAL_COM1, "[elf] File not found: %s\n", path);
        return -1;
    }

    /* 2. 读取 ELF 头 */
    Elf32_Ehdr ehdr;
    int n = read_file_at(ino, (u8 *)&ehdr, sizeof(Elf32_Ehdr), 0);
    if (n < (int)sizeof(Elf32_Ehdr)) {
        serial_printf(SERIAL_COM1, "[elf] Failed to read ELF header\n");
        return -1;
    }

    /* 3. 验证 ELF */
    if (!elf_check_file(&ehdr)) {
        serial_printf(SERIAL_COM1, "[elf] Invalid ELF file\n");
        return -1;
    }

    serial_printf(SERIAL_COM1, "[elf] Entry=0x%x, phoff=%d, phnum=%d\n",
                  ehdr.e_entry, ehdr.e_phoff, ehdr.e_phnum);

    /* 4. 加载每个 PT_LOAD 段 */
    for (u16 i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        u32 phdr_offset = ehdr.e_phoff + i * sizeof(Elf32_Phdr);
        n = read_file_at(ino, (u8 *)&phdr, sizeof(Elf32_Phdr), phdr_offset);
        if (n < (int)sizeof(Elf32_Phdr)) {
            serial_printf(SERIAL_COM1, "[elf] Failed to read phdr %d\n", i);
            return -1;
        }

        if (phdr.p_type != PT_LOAD) continue;

        serial_printf(SERIAL_COM1, "[elf]  LOAD: vaddr=0x%x, filesz=%d, memsz=%d, flags=0x%x\n",
                      phdr.p_vaddr, phdr.p_filesz, phdr.p_memsz, phdr.p_flags);

        if (load_segment(ino, phdr.p_vaddr, phdr.p_offset,
                         phdr.p_filesz, phdr.p_memsz) < 0) {
            serial_printf(SERIAL_COM1, "[elf] Failed to load segment %d\n", i);
            return -1;
        }
    }

    /* 5. 设置用户栈 */
    setup_user_stack();

    /* 6. 修改寄存器状态，使 syscall 返回到用户程序入口 */
    /* 当前 regs 包含用户态执行 int 0x80 时的状态 */
    /* iret 将使用 regs->eip, cs, eflags, user_esp, user_ss */
    regs->eip      = ehdr.e_entry;
    regs->user_esp = USER_STACK_VADDR;   /* 栈顶（向下增长） */
    /* cs=0x1B (用户代码段, RPL=3) 保持不变 */
    /* ss=0x23 (用户数据段, RPL=3) 保持不变 */
    regs->eflags  |= 0x200;              /* 确保 IF=1（中断使能） */

    /* 7. 清零通用寄存器（干净的启动环境） */
    regs->eax = 0;
    regs->ebx = 0;
    regs->ecx = 0;
    regs->edx = 0;
    regs->esi = 0;
    regs->edi = 0;
    regs->ebp = 0;
    /* regs->esp 由 IRET 的 user_esp 决定 */

    serial_printf(SERIAL_COM1, "[elf] Executing at 0x%x, stack=0x%x\n",
                  ehdr.e_entry, USER_STACK_VADDR);

    /* 成功：返回 0，此时 regs 已修改，syscall 返回时会跳转到用户程序 */
    return 0;
}
