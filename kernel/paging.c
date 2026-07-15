#include "paging.h"
#include "types.h"
#include "idt.h"
#include "vga.h"
#include "serial.h"

/* kernel_after_paging 定义在 kernel.c 中，由 paging_init 调用（不返回） */
extern void kernel_after_paging(void) __attribute__((noreturn));

#define MAX_PAGES 1024

static u32 page_frames[MAX_PAGES];
static int page_frame_count = 0;
void pfa_push(u32 phys_addr){
    page_frames[page_frame_count++] = phys_addr;
}

u32 pfa_alloc(void){
    if(page_frame_count == 0) panic("Out of memory!");
    return page_frames[--page_frame_count];
}

void pfa_free(u32 phys_addr){
    page_frames[page_frame_count++] = phys_addr;
}

void paging_init(){
    u32 *page_dir = (u32 *) pfa_alloc();
    memset(page_dir, 0, 4096);
    u32 *page_table = NULL;
    for(u32 addr = 0; addr < 0x400000; addr += 0x400000){
        int pd_idx = addr >> 22;
        page_table = (u32 *) pfa_alloc();
        memset(page_table, 0, 4096);

        for(int i = 0; i < 1024; i++){
            u32 phys = addr + i * 4096;
            page_table[i] = phys | 0x03;   /* P+W，不设 User 位——Ring3 不可访问 */
        }
        page_dir[pd_idx] = ((u32) page_table) | 0x03;
    }

    /* 修正 VGA 恒等映射：虚拟 0xB8000 → 物理 0xB8000
     * pt_idx = (0xB8000>>12) & 0x3FF = 0x380，循环中默认指向 0x380000。
     * 覆盖为 VGA 显存物理地址 0xB8000。 */
    {
        u32 vga_pt_idx = (0xB8000 >> 12) & 0x3FF;
        u32 *pt0 = (u32 *)(page_dir[0] & 0xFFFFF000);
        pt0[vga_pt_idx] = 0xB8000 | 0x03;
    }

    asm volatile("mov %0, %%cr3" ::"r"(page_dir));
    u32 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" ::"r"(cr0));
    page_dir[1023] = (u32)page_dir | 0x03;

    /* 不返回，直接继续内核初始化 */
    kernel_after_paging();
    __builtin_unreachable();
}
void page_fault_handler(registers_t *regs) {
    u32 fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    vga_printf("Page Fault! Virtual Address=0x%x\n", fault_addr);
    vga_printf("  EIP=0x%x, Error Code=0x%x\n", regs->eip, regs->err_code);
    serial_printf(SERIAL_COM1, "Page Fault! Virtual Address=0x%x\n", fault_addr);
    serial_printf(SERIAL_COM1, " EIP=0x%x, Error Code=0x%x\n", regs->eip, regs->err_code);
    // 错误码分析：
    // 位 0 (P) — 0=页不存在, 1=页保护违规
    // 位 1 (W) — 0=读操作, 1=写操作
    // 位 2 (U) — 0=内核态, 1=用户态
    panic("Page fault!");
}
void paging_map_user(u32 virt_addr)
{
    u32 *page_dir;
    __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir));

    u32 pd_idx = virt_addr >> 22;
    u32 pt_idx = (virt_addr >> 12) & 0x3FF;

    u32 *page_table = (u32 *)(page_dir[pd_idx] & 0xFFFFF000);

    page_table[pt_idx] |= 0x04;    /* 页表项设置 U/S */
    page_dir[pd_idx] |= 0x04;      /* 页目录项设置 U/S */

    __asm__ volatile("invlpg %0" : : "m"(*(u32 *)virt_addr));
}

/* 映射物理页 phys_addr 到虚拟地址 virt_addr，带指定权限标志 */
void paging_map_page(u32 virt_addr, u32 phys_addr, u8 flags)
{
    u32 *page_dir;
    __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir));

    u32 pd_idx = virt_addr >> 22;
    u32 pt_idx = (virt_addr >> 12) & 0x3FF;

    if (!(page_dir[pd_idx] & 0x01)) {
        /* 页目录项不存在，分配新页表 */
        u32 pt_phys = pfa_alloc();

        /* 先设置页目录项（带 Present 位），使自映射可访问 */
        page_dir[pd_idx] = (pt_phys & 0xFFFFF000) | 0x01;

        /* 通过自映射清零新页表 */
        u32 *virt_pt = (u32 *)(0xFFC00000 + pd_idx * 4096);
        for (int i = 0; i < 1024; i++)
            virt_pt[i] = 0;

        /* 更新页目录项为最终权限 */
        page_dir[pd_idx] = (pt_phys & 0xFFFFF000) | (flags & 0x07) | 0x01;
    }

    u32 *page_table = (u32 *)(0xFFC00000 + pd_idx * 4096);
    page_table[pt_idx] = (phys_addr & 0xFFFFF000) | (flags & 0x07) | 0x01;

    __asm__ volatile("invlpg %0" : : "m"(*(u32 *)virt_addr));
}

/* 分配一页物理内存并映射到指定虚拟地址（用户可访问） */
void paging_alloc_page(u32 virt_addr)
{
    u32 phys = pfa_alloc();
    paging_map_page(virt_addr, phys, 0x07); /* Present + Writable + User */
}

/* 克隆当前页目录：深度复制所有用户页面，为子进程创建独立地址空间 */
u32 paging_clone_directory(void)
{
    u32 *old_pd;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_pd));

    /* 1. 分配新页目录 */
    u32 new_pd_phys = pfa_alloc();
    u32 *new_pd = (u32 *)new_pd_phys;

    /* 通过自映射访问新旧页目录/页表 */
    /* 自映射区域: 0xFFC00000-0xFFFFFFFF
     *   0xFFC00000 + pd_idx*4096  → 页表 pd_idx 的虚拟地址
     *   0xFFFFF000                 → 页目录自身的虚拟地址 */

    /* 内核二进制范围：_bss_end 是内核 BSS 结束地址，向上取整到页边界 */
    extern u32 _bss_end;
    u32 kernel_end_page = ((u32)&_bss_end + 0xFFF) & 0xFFFFF000;

    for (int pd_idx = 0; pd_idx < 1023; pd_idx++) {
        u32 old_pde = old_pd[pd_idx];

        if (!(old_pde & 0x01)) {
            /* PDE 不存在 */
            new_pd[pd_idx] = 0;
            continue;
        }

        /* PDE 0-1 是恒等映射的内核区域（0-8MB）。
         * 内核二进制页（0x100000 ~ _bss_end）直接共享。
         * 内核二进制范围外且 User 位置位的页（如用户栈）
         * 需要深度复制以创建独立副本。 */
        if (pd_idx <= 1) {
            u32 *old_pt = (u32 *)(0xFFC00000 + pd_idx * 4096);
            int has_user_pages_outside_kernel = 0;
            for (int pti = 0; pti < 1024; pti++) {
                if (!(old_pt[pti] & 0x01)) continue;
                if (!(old_pt[pti] & 0x04)) continue;
                u32 page_vaddr = (pd_idx << 22) | (pti << 12);
                /* 跳过内核二进制范围内的页 */
                if (page_vaddr >= 0x100000 && page_vaddr < kernel_end_page)
                    continue;
                has_user_pages_outside_kernel = 1;
                break;
            }
            if (!has_user_pages_outside_kernel) {
                /* 没有需要复制的外部用户页，直接共享页表 */
                new_pd[pd_idx] = old_pde;
                continue;
            }
            /* 有需要复制的用户页：深度复制页表 */
            u32 new_pt_phys = pfa_alloc();
            u32 *new_pt = (u32 *)new_pt_phys;
            for (int pti = 0; pti < 1024; pti++)
                new_pt[pti] = 0;
            for (int pti = 0; pti < 1024; pti++) {
                u32 old_pte = old_pt[pti];
                if (!(old_pte & 0x01)) continue;
                u32 page_vaddr = (pd_idx << 22) | (pti << 12);
                /* 内核二进制范围内的页：直接共享 */
                if (page_vaddr >= 0x100000 && page_vaddr < kernel_end_page) {
                    new_pt[pti] = old_pte;
                    continue;
                }
                if (old_pte & 0x04) {
                    /* 用户页（非内核二进制范围）：分配新物理页并复制 */
                    u32 new_page_phys = pfa_alloc();
                    u32 old_page_phys = old_pte & 0xFFFFF000;
                    memcpy((void *)new_page_phys, (void *)old_page_phys, 4096);
                    new_pt[pti] = (new_page_phys & 0xFFFFF000) | (old_pte & 0xFFF);
                } else {
                    /* 内核页（非内核二进制）：共享 */
                    new_pt[pti] = old_pte;
                }
            }
            new_pd[pd_idx] = (new_pt_phys & 0xFFFFF000) | (old_pde & 0xFFF);
            continue;
        }

        if (old_pde & 0x04) {
            /* 用户 PDE：深度复制页表 */
            u32 new_pt_phys = pfa_alloc();

            /* 通过自映射获取旧页表虚拟地址 */
            u32 *old_pt = (u32 *)(0xFFC00000 + pd_idx * 4096);

            /* 新页表物理地址，先清零 */
            u32 *new_pt = (u32 *)new_pt_phys;
            for (int i = 0; i < 1024; i++)
                new_pt[i] = 0;

            /* 遍历 PTE */
            for (int pt_idx = 0; pt_idx < 1024; pt_idx++) {
                u32 old_pte = old_pt[pt_idx];
                if (!(old_pte & 0x01))
                    continue; /* PTE 不存在 */

                /* 检查 PTE 的虚拟地址是否在恒等映射区域内（0-8MB）
                 * 如果是，共享该页而非深度复制 */
                u32 page_vaddr = (pd_idx << 22) | (pt_idx << 12);
                if (page_vaddr < 0x800000) {
                    new_pt[pt_idx] = old_pte;
                    continue;
                }

                if (old_pte & 0x04) {
                    /* 用户页：分配新物理页并复制内容 */
                    u32 new_page_phys = pfa_alloc();

                    /* 通过恒等映射访问源页（0-8MB 恒等映射） */
                    u32 old_page_phys = old_pte & 0xFFFFF000;

                    /* 复制页面内容 */
                    memcpy((void *)new_page_phys, (void *)old_page_phys, 4096);

                    /* 设置新 PTE：保留原标志位但替换物理地址 */
                    new_pt[pt_idx] = (new_page_phys & 0xFFFFF000)
                                   | (old_pte & 0xFFF);
                } else {
                    /* 内核页：共享（直接复制 PTE） */
                    new_pt[pt_idx] = old_pte;
                }
            }

            /* 设置新 PDE */
            new_pd[pd_idx] = (new_pt_phys & 0xFFFFF000)
                           | (old_pde & 0xFFF);
        } else {
            /* 内核 PDE：直接共享页表 */
            new_pd[pd_idx] = old_pde;
        }
    }

    /* 自映射 PDE[1023] 指向新页目录自身 */
    new_pd[1023] = (new_pd_phys & 0xFFFFF000) | 0x03;

    serial_printf(SERIAL_COM1, "[paging] Cloned page directory: "
                  "new=0x%x\n", new_pd_phys);
    return new_pd_phys;
}