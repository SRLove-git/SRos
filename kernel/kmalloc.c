#include "kmalloc.h"
#include "types.h"
#include "serial.h"

/* ── 堆区域配置 ── */
/* HEAP_START 选择 2MB 处，原因：
 * - 内核加载在 1MB，内核结束约 0x108A30
 * - 页表已恒等映射 0~4MB（见 paging_init）
 * - 2MB 处完全是空闲的虚拟地址空间
 */
#define HEAP_START  0x200000
#define HEAP_SIZE   0x40000     /* 256KB 堆空间 */
#define ALIGN       8           /* 8 字节对齐 */
#define MIN_BLOCK   16          /* 最小块大小（Header+Footer+至少8字节数据） */

/* ── 块头部结构 ── */
/* 每个内存块（无论空闲/已分配）都以 header 开头：
 *   size 的高位存储块大小（含 header 和 footer）
 *   低 3 位作为标记位：bit0=1 已分配，bit0=0 空闲
 *   低 3 位可安全使用，因为块大小始终 8 字节对齐
 */
typedef struct block_header {
    u32 size;
} __attribute__((packed)) block_header_t;

#define HEADER_SIZE  sizeof(block_header_t)   /* 4 字节 */

/* ── 块操作宏 ── */
/* 提取块大小（清除低 3 位标记） */
#define BLOCK_SIZE(hdr)     ((hdr)->size & ~0x7)

/* 标记操作 */
#define BLOCK_IS_ALLOC(hdr) ((hdr)->size & 1)
#define BLOCK_SET_ALLOC(hdr)   ((hdr)->size |= 1)
#define BLOCK_SET_FREE(hdr)    ((hdr)->size &= ~1)

/* 定位下一个块 */
#define NEXT_BLOCK(hdr) \
    ((block_header_t *)((u32)(hdr) + BLOCK_SIZE(hdr)))

/* 定位当前块的 Footer（在块末尾，与 Header 内容相同） */
#define BLOCK_FOOTER(hdr) \
    ((block_header_t *)((u32)(hdr) + BLOCK_SIZE(hdr) - HEADER_SIZE))

/* 定位前一个块：通过当前块 header 前 4 字节找到前一块的 Footer */
#define PREV_BLOCK(hdr)                                             \
    ((block_header_t *)((u32)(hdr) -                                \
        BLOCK_SIZE((block_header_t *)((u32)(hdr) - HEADER_SIZE))))



void kmalloc_init(void) {
    // 1. 第一个空闲块的 Header
    block_header_t *first = (block_header_t *)HEAP_START;
    first->size = HEAP_SIZE;          // 空闲，低 3 位都是 0

    // 2. 第一个空闲块的 Footer（与 Header 内容相同）
    block_header_t *footer = (block_header_t *)(HEAP_START + HEAP_SIZE - HEADER_SIZE);
    footer->size = HEAP_SIZE;

    // 3. 终止块
    block_header_t *epilogue = (block_header_t *)(HEAP_START + HEAP_SIZE);
    epilogue->size = 1;               // size=0 且标记为已分配 ...
}

void *kmalloc(u32 size) {
    if (size == 0) return NULL;

    // 1. 向上对齐到 8 字节
    // 比如 size=5 → (5+7) & ~7 = 12 & ~7 = 8
    u32 aligned_size = (size + ALIGN - 1) & ~(ALIGN - 1);

    // 2. 计算实际块大小（Header + 数据 + Footer）
    u32 need_size = HEADER_SIZE + aligned_size + HEADER_SIZE;
    if (need_size < MIN_BLOCK)
        need_size = MIN_BLOCK;
    
    // 3. 从堆头开始遍历
    block_header_t *cur = (block_header_t *)HEAP_START;
    
    while (1) {
        u32 cur_size = BLOCK_SIZE(cur);
        
        // 遇到终止块（size=0）说明到堆尾了
        if (cur_size == 0)
            return NULL;
        
        if (!BLOCK_IS_ALLOC(cur) && cur_size >= need_size) {
            // ── 找到了合适的空闲块！──
            
            u32 remaining = cur_size - need_size;
            
            if (remaining >= MIN_BLOCK) {
                /* 分割：当前块取 need_size，剩余部分形成新的空闲块 */
                // 设置当前块为已分配（大小为 need_size）
                cur->size = need_size;
                BLOCK_SET_ALLOC(cur);
                BLOCK_FOOTER(cur)->size = need_size;
                BLOCK_SET_ALLOC(BLOCK_FOOTER(cur));
                
                // 创建新的空闲块（紧接在当前块之后）
                block_header_t *new_free = (block_header_t *)((u32)cur + need_size);
                new_free->size = remaining;           // 空闲（最低位=0）
                BLOCK_FOOTER(new_free)->size = remaining;
            } else {
                /* 剩余空间太小，整个块都给出去，不分割 */
                cur->size = cur_size;
                BLOCK_SET_ALLOC(cur);
                BLOCK_FOOTER(cur)->size = cur_size;
                BLOCK_SET_ALLOC(BLOCK_FOOTER(cur));
            }
            
            // 返回数据区起始地址（Header 之后）
            return (void *)((u32)cur + HEADER_SIZE);
        }
        
        // 移动到下一个块
        cur = NEXT_BLOCK(cur);
    }
}

void kfree(void *ptr) {
    if (ptr == NULL) return;

    // 1. 从数据指针找到 Header
    block_header_t *hdr = (block_header_t *)((u32)ptr - HEADER_SIZE);

    // 2. 标记为空闲，同步 Footer
    BLOCK_SET_FREE(hdr);
    BLOCK_FOOTER(hdr)->size = hdr->size;

    // 3. 向后合并：后一块空闲则合并
    block_header_t *next = NEXT_BLOCK(hdr);
    if (BLOCK_SIZE(next) > 0 && !BLOCK_IS_ALLOC(next)) {
        u32 new_size = BLOCK_SIZE(hdr) + BLOCK_SIZE(next);
        hdr->size = new_size;                   // 仍为空闲（bit0=0）
        BLOCK_FOOTER(hdr)->size = new_size;     // 同步 Footer
    }

    // 4. 向前合并：前一块空闲则合并（利用 Footer 实现）
    if ((u32)hdr > HEAP_START) {
        block_header_t *prev = PREV_BLOCK(hdr);
        if (!BLOCK_IS_ALLOC(prev)) {
            u32 new_size = BLOCK_SIZE(prev) + BLOCK_SIZE(hdr);
            prev->size = new_size;
            BLOCK_FOOTER(prev)->size = new_size;
        }
    }
}

void kmalloc_dump(void) {
    block_header_t *cur = (block_header_t *)HEAP_START;
    u32 total_free = 0;
    u32 total_alloc = 0;
    int blocks = 0;

    serial_printf(SERIAL_COM1, "=== Heap Dump ===\n");
    serial_printf(SERIAL_COM1, "HEAP_START=0x%x, HEAP_SIZE=0x%x\n",
                  HEAP_START, HEAP_SIZE);

    while (1) {
        u32 cur_size = BLOCK_SIZE(cur);
        if (cur_size == 0) break;  // 遇到终止块

        serial_printf(SERIAL_COM1, "  [0x%x] size=%d %s\n",
                      cur, cur_size,
                      BLOCK_IS_ALLOC(cur) ? "ALLOC" : "FREE");

        if (BLOCK_IS_ALLOC(cur))
            total_alloc += cur_size;
        else
            total_free += cur_size;

        blocks++;
        cur = NEXT_BLOCK(cur);
    }

    serial_printf(SERIAL_COM1, "Total: %d blocks, %d alloc, %d free\n",
                  blocks, total_alloc, total_free);
    serial_printf(SERIAL_COM1, "=== End ===\n");
}