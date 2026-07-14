#include "syscall.h"
#include "idt.h"
#include "keyboard.h"
#include "vga.h"
#include "serial.h"
#include "sfs.h"
#include "ramdisk.h"
#include "kmalloc.h"
#include "string.h"

void syscall_handler(registers_t *regs)
{
    u32 syscall_num = regs->eax;

    switch (syscall_num) {

    case SYS_READ: {
        char *buf = (char *)regs->ebx;
        u32 len = regs->ecx;
        u32 count = 0;

        while (count < len && kbd_buffer_has_data())
            buf[count++] = kbd_buffer_get();
        regs->eax = count;
        break;
    }

    case SYS_GETCHAR: {
        while (!kbd_buffer_has_data())
            __asm__ volatile("hlt");
        regs->eax = kbd_buffer_get();
        break;
    }

    case SYS_WRITE: {
        vga_putc((char)regs->ebx);
        regs->eax = 0;
        break;
    }

    case SYS_CLEAR: {
        vga_init();
        regs->eax = 0;
        break;
    }

    /* ===== 文件系统系统调用 ===== */

    case SYS_FS_OPEN: {
        /* ebx = path (user-space string pointer) */
        const char *path = (const char *)regs->ebx;
        regs->eax = sys_open(path);
        break;
    }

    case SYS_FS_READ: {
        /* ebx = fd, ecx = buf, edx = count */
        int fd = (int)regs->ebx;
        char *buf = (char *)regs->ecx;
        int count = (int)regs->edx;
        regs->eax = sys_read(fd, buf, count);
        break;
    }

    case SYS_FS_WRITE: {
        /* ebx = fd, ecx = buf, edx = count */
        int fd = (int)regs->ebx;
        const char *buf = (const char *)regs->ecx;
        int count = (int)regs->edx;
        regs->eax = sys_write(fd, buf, count);
        break;
    }

    case SYS_FS_CLOSE: {
        /* ebx = fd */
        int fd = (int)regs->ebx;
        sys_close(fd);
        regs->eax = 0;
        break;
    }

    case SYS_FS_CREATE: {
        /* ebx = filename (user-space string pointer) */
        const char *name = (const char *)regs->ebx;
        int ino = sfs_create(ROOT_INO, name, SFS_FILE);
        regs->eax = (ino >= 0) ? 0 : -1;
        break;
    }

    case SYS_FS_LS: {
        /* ebx = user-space buf, ecx = buf size */
        /* 将根目录文件名列表写入用户缓冲区 */
        char *user_buf = (char *)regs->ebx;
        u32 buf_size = regs->ecx;
        u32 written = 0;
        int first = 1;

        if (!user_buf || buf_size == 0) { regs->eax = -1; break; }

        struct sfs_inode root_inode;
        sfs_read_inode(ROOT_INO, &root_inode);

        for (u32 bi = 0; bi < root_inode.block_count; bi++) {
            u8 tmp_block[BLOCK_SIZE];
            block_read(root_inode.blocks[bi], tmp_block);
            u32 dentries_per_block = BLOCK_SIZE / sizeof(struct dentry);
            for (u32 di = 0; di < dentries_per_block; di++) {
                struct dentry d;
                u32 off = di * sizeof(struct dentry);
                for (u32 i = 0; i < sizeof(struct dentry); i++)
                    ((u8 *)&d)[i] = tmp_block[off + i];
                if (d.ino != 0 && strcmp(d.name, ".") != 0 && strcmp(d.name, "..") != 0) {
                    /* 写入分隔符 */
                    if (!first && written + 1 < buf_size) {
                        user_buf[written++] = '\n';
                    }
                    first = 0;
                    /* 写入文件名 */
                    for (u32 i = 0; d.name[i] && written + 1 < buf_size; i++)
                        user_buf[written++] = d.name[i];
                }
            }
        }
        if (written < buf_size)
            user_buf[written] = '\0';
        regs->eax = written;
        break;
    }

    /* ===== 堆内存系统调用 ===== */

    case SYS_KMALLOC: {
        /* ebx = size */
        u32 size = regs->ebx;
        regs->eax = (u32)kmalloc(size);
        break;
    }

    case SYS_KFREE: {
        /* ebx = pointer */
        kfree((void *)regs->ebx);
        regs->eax = 0;
        break;
    }

    case SYS_KMALLOC_DUMP: {
        kmalloc_dump();
        regs->eax = 0;
        break;
    }

    default:
        regs->eax = -1;
        break;
    }
}
