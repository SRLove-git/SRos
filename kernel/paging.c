#include "paging.h"
#include "types.h"
#include "idt.h"
#include "vga.h"
#include "serial.h"
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
    for(u32 addr = 0; addr < 0x400000; addr += 0x400000){
        int pd_idx = addr >> 22;
        u32 *page_table = (u32 *) pfa_alloc();
        memset(page_table, 0, 4096);

        for(int i = 0; i < 1024; i++){
            u32 phys = addr + i * 4096;
            page_table[i] = phys | 0x03;
        }
        page_dir[pd_idx] = ((u32) page_table) | 0x03;
    }
    asm volatile("mov %0, %%cr3" ::"r"(page_dir));
    u32 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" ::"r"(cr0));
    page_dir[1023] = (u32)page_dir | 0x03;
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