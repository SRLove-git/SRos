#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

/* 默认分辨率 */
#define FB_DEFAULT_WIDTH  1024
#define FB_DEFAULT_HEIGHT 768
#define FB_DEFAULT_BPP    32

/* 帧缓冲全局信息 */
typedef struct {
    u32    width;
    u32    height;
    u32    pitch;        /* 每行字节数 */
    u8     bpp;          /* 每像素位数 */
    u32   *addr;         /* 映射后的虚拟地址 */
    u32    phys_addr;    /* 物理地址 */
} framebuffer_t;

extern framebuffer_t fb;

/* 颜色结构 */
typedef struct {
    u8 r, g, b, a;
} color_t;

/* 常用颜色 */
#define COLOR_BLACK     ((color_t){0, 0, 0, 0})
#define COLOR_WHITE     ((color_t){255, 255, 255, 0})
#define COLOR_RED       ((color_t){255, 0, 0, 0})
#define COLOR_GREEN     ((color_t){0, 255, 0, 0})
#define COLOR_BLUE      ((color_t){0, 0, 255, 0})
#define COLOR_YELLOW    ((color_t){255, 255, 0, 0})
#define COLOR_CYAN      ((color_t){0, 255, 255, 0})
#define COLOR_MAGENTA   ((color_t){255, 0, 255, 0})
#define COLOR_GRAY      ((color_t){128, 128, 128, 0})
#define COLOR_ORANGE    ((color_t){255, 165, 0, 0})

/* ── 初始化 ── */
void framebuffer_init(u32 phys_addr, u32 width, u32 height, u32 pitch, u8 bpp);
void framebuffer_map(void);

/* 使用 Bochs VBE I/O 端口初始化 VBE 图形模式 */
/* 返回值：0=成功，-1=失败（无 VBE 支持） */
int vbe_init(u32 width, u32 height, u8 bpp);

/* ── 像素操作 ── */
void fb_putpixel(u32 x, u32 y, color_t c);
void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, color_t c);
void fb_clear(color_t c);

/* ── 字符/文本输出 ── */
void fb_putchar(int c, u32 x, u32 y, color_t fg, color_t bg);
void fb_puts(const char *s, u32 x, u32 y, color_t fg, color_t bg);
void fb_printf(const char *fmt, ...);

/* ── 帧缓冲控制台 ── */
void fb_console_init(void);
void fb_console_putc(int c);
void fb_console_puts(const char *s);
void fb_console_printf(const char *fmt, ...);

#endif /* FRAMEBUFFER_H */
