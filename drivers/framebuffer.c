#include "framebuffer.h"
#include "vgafont.h"
#include "paging.h"
#include "string.h"
#include "serial.h"
#include "stdarg.h"
#include "io.h"

/* Bochs VBE I/O 端口 */
#define VBE_DISPI_IOPORT_INDEX  0x01CE
#define VBE_DISPI_IOPORT_DATA   0x01CF

/* Bochs VBE 索引寄存器 */
#define VBE_DISPI_INDEX_ID        0x00
#define VBE_DISPI_INDEX_XRES      0x01
#define VBE_DISPI_INDEX_YRES      0x02
#define VBE_DISPI_INDEX_BPP       0x03
#define VBE_DISPI_INDEX_ENABLE    0x04
#define VBE_DISPI_INDEX_BANK      0x05
#define VBE_DISPI_INDEX_VIRT_WIDTH  0x06
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x07
#define VBE_DISPI_INDEX_X_OFFSET    0x08
#define VBE_DISPI_INDEX_Y_OFFSET    0x09

/* Bochs VBE 标志 */
#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40

/* Bochs VBE 支持的 ID */
#define VBE_DISPI_ID_MIN       0xB0C0
#define VBE_DISPI_ID_MAX       0xB0C5

/* QEMU 中 Bochs VBE 的默认 LFB 物理地址 */
/* 注意：实际地址应从 PCI BAR0 读取，这里作为 fallback */
#define VBE_DISPI_LFB_PHYSICAL 0xE0000000

/* PCI 配置空间读取（通过 I/O 端口 0xCF8/0xCFC） */
#define PCI_CONFIG_ADDRESS  0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

static u32 pci_config_read(u8 bus, u8 dev, u8 func, u8 offset)
{
    u32 address = (u32)0x80000000
                | ((u32)bus << 16)
                | ((u32)dev << 11)
                | ((u32)func << 8)
                | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/* 枚举 PCI 设备，查找 VGA 兼容设备，返回其 BAR0 地址 */
static u32 pci_find_vga_lfb(void)
{
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 dev = 0; dev < 32; dev++) {
            u16 vendor = (u16)(pci_config_read(bus, dev, 0, 0) & 0xFFFF);
            if (vendor == 0xFFFF || vendor == 0x0000)
                continue;

            u32 class_reg = pci_config_read(bus, dev, 0, 8);
            u8 class_code = (class_reg >> 24) & 0xFF;
            u8 subclass  = (class_reg >> 16) & 0xFF;

            /* VGA 兼容设备：class=0x03, subclass=0x00 */
            if (class_code == 0x03 && subclass == 0x00) {
                u32 bar0 = pci_config_read(bus, dev, 0, 0x10);
                serial_printf(SERIAL_COM1, "[pci] VGA device %d:%d BAR0=0x%x\n",
                              bus, dev, bar0);
                /* BAR0 的低位是标志位，屏蔽掉 */
                u32 lfb_addr = bar0 & 0xFFFFFFF0;
                if (lfb_addr && lfb_addr != 0xFFFFFFF0)
                    return lfb_addr;
            }
        }
    }
    return 0;  /* 未找到 */
}

/* Bochs VBE 辅助函数：写索引寄存器 */
static inline void vbe_write(u16 index, u16 value)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

/* Bochs VBE 辅助函数：读数据寄存器 */
static inline u16 vbe_read(u16 index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/* 使用 Bochs VBE I/O 端口初始化图形模式 */
int vbe_init(u32 width, u32 height, u8 bpp)
{
    /* 检查 Bochs VBE 是否存在 */
    u16 id = vbe_read(VBE_DISPI_INDEX_ID);
    if (id < VBE_DISPI_ID_MIN || id > VBE_DISPI_ID_MAX) {
        serial_printf(SERIAL_COM1, "[vbe] not found (id=0x%x)\n", id);
        return -1;
    }
    serial_printf(SERIAL_COM1, "[vbe] found (id=0x%x), initializing %d x %d x %dbpp\n",
                  id, width, height, bpp);

    /* 1. 先关闭显示 */
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    /* 2. 设置分辨率 */
    vbe_write(VBE_DISPI_INDEX_XRES, (u16)width);
    vbe_write(VBE_DISPI_INDEX_YRES, (u16)height);
    vbe_write(VBE_DISPI_INDEX_BPP, (u16)bpp);
    vbe_write(VBE_DISPI_INDEX_VIRT_WIDTH, (u16)width);
    vbe_write(VBE_DISPI_INDEX_VIRT_HEIGHT, (u16)height);

    /* 3. 启用 LFB 模式 */
    vbe_write(VBE_DISPI_INDEX_ENABLE,
              VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    /* 4. 验证模式是否设置成功 */
    u16 xres = vbe_read(VBE_DISPI_INDEX_XRES);
    u16 yres = vbe_read(VBE_DISPI_INDEX_YRES);
    u16 bpp_actual = vbe_read(VBE_DISPI_INDEX_BPP);

    serial_printf(SERIAL_COM1, "[vbe] mode set: %dx%d %dbpp (actual: %dx%d %dbpp)\n",
                  width, height, bpp, xres, yres, bpp_actual);

    if (xres == 0 || yres == 0) {
        serial_printf(SERIAL_COM1, "[vbe] mode set failed!\n");
        return -1;
    }

    /* 5. 初始化帧缓冲信息（优先使用 PCI BAR0 地址） */
    u32 lfb_phys = pci_find_vga_lfb();
    if (lfb_phys == 0) {
        lfb_phys = VBE_DISPI_LFB_PHYSICAL;  /* fallback */
        serial_printf(SERIAL_COM1, "[vbe] using default LFB address 0x%x\n", lfb_phys);
    } else {
        serial_printf(SERIAL_COM1, "[vbe] using PCI BAR0 LFB address 0x%x\n", lfb_phys);
    }

    framebuffer_init(lfb_phys, xres, yres,
                     xres * (bpp_actual / 8), bpp_actual);

    serial_printf(SERIAL_COM1, "[vbe] LFB at 0x%x, pitch=%d\n",
                  lfb_phys, fb.pitch);

    return 0;
}

/* ── 全局帧缓冲信息 ── */
framebuffer_t fb;

/* ── 控制台状态 ── */
#define CONSOLE_COLS  (fb.width / FONT_CHAR_WIDTH)
#define CONSOLE_ROWS  (fb.height / FONT_CHAR_HEIGHT)
static int console_row = 0;
static int console_col = 0;
static color_t console_fg = COLOR_WHITE;
static color_t console_bg = COLOR_BLACK;

/* 32位像素：B=byte[0], G=byte[1], R=byte[2], A=byte[3] */
static inline u32 color_to_pixel(color_t c)
{
    return ((u32)c.r << 16) | ((u32)c.g << 8) | (u32)c.b;
}

/* ── 初始化 ── */
void framebuffer_init(u32 phys_addr, u32 width, u32 height, u32 pitch, u8 bpp)
{
    fb.phys_addr = phys_addr;
    fb.width     = width;
    fb.height    = height;
    fb.pitch     = pitch;
    fb.bpp       = bpp;
    fb.addr      = NULL; /* 还未映射 */
}

/* 将帧缓冲物理地址映射到内核虚拟地址空间 */
void framebuffer_map(void)
{
    if (fb.addr) return; /* 已映射 */

    u32 fb_size = fb.pitch * fb.height;
    u32 fb_pages = (fb_size + 0xFFF) / 0x1000;

    /* 选择一个不会被其他映射使用的虚拟地址 */
    /* 使用 0xF0000000 之后的区域 */
    u32 virt_base = 0xF0000000;
    u32 phys = fb.phys_addr;

    for (u32 i = 0; i < fb_pages; i++) {
        paging_map_page(virt_base + i * 0x1000,
                        phys + i * 0x1000,
                        0x07); /* Present + Writable + User */
    }

    fb.addr = (u32 *)virt_base;
    serial_printf(SERIAL_COM1, "[fb] mapped 0x%x -> 0x%x (%u pages)\n",
                  phys, virt_base, fb_pages);
    serial_printf(SERIAL_COM1, "[fb] %dx%d %dbpp pitch=%d\n",
                  fb.width, fb.height, fb.bpp, fb.pitch);
}

/* ── 像素操作 ── */
void fb_putpixel(u32 x, u32 y, color_t c)
{
    if (x >= fb.width || y >= fb.height) return;
    if (!fb.addr) return;

    u32 pixel = color_to_pixel(c);
    u32 *ptr = fb.addr + (y * (fb.pitch / 4) + x);
    *ptr = pixel;
}

void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, color_t c)
{
    if (!fb.addr) return;

    /* 裁剪 */
    if (x >= fb.width || y >= fb.height) return;
    if (x + w > fb.width)  w = fb.width - x;
    if (y + h > fb.height) h = fb.height - y;

    u32 pixel = color_to_pixel(c);
    u32 pitch_dwords = fb.pitch / 4;

    for (u32 row = 0; row < h; row++) {
        u32 *ptr = fb.addr + (y + row) * pitch_dwords + x;
        for (u32 col = 0; col < w; col++) {
            ptr[col] = pixel;
        }
    }
}

void fb_clear(color_t c)
{
    fb_fill_rect(0, 0, fb.width, fb.height, c);
}

/* ── 字符绘制 ── */
void fb_putchar(int c, u32 x, u32 y, color_t fg, color_t bg)
{
    if (!fb.addr) return;
    if (x + FONT_CHAR_WIDTH > fb.width) return;
    if (y + FONT_CHAR_HEIGHT > fb.height) return;

    u32 fg_pixel = color_to_pixel(fg);
    u32 bg_pixel = color_to_pixel(bg);
    u32 pitch_dwords = fb.pitch / 4;

    if (c < FONT_FIRST_CHAR || c >= FONT_FIRST_CHAR + FONT_NUM_CHARS)
        c = ' ';
    int idx = c - FONT_FIRST_CHAR;

    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        u8 bits = font_vga8x16[idx][row];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            u32 pixel = (bits & (0x80 >> col)) ? fg_pixel : bg_pixel;
            fb.addr[(y + row) * pitch_dwords + (x + col)] = pixel;
        }
    }
}

/* 在指定位置输出字符串 */
void fb_puts(const char *s, u32 x, u32 y, color_t fg, color_t bg)
{
    while (*s) {
        fb_putchar(*s, x, y, fg, bg);
        x += FONT_CHAR_WIDTH;
        if (x + FONT_CHAR_WIDTH > fb.width) break;
        s++;
    }
}

/* ── 帧缓冲控制台 ── */

/* 滚动屏幕：将所有行上移一行 */
static void fb_console_scroll(void)
{
    u32 pitch_dwords = fb.pitch / 4;
    u32 row_bytes = fb.pitch;

    /* 将第1行开始的每一行复制到上一行位置 */
    u32 total_rows = CONSOLE_ROWS;
    for (u32 row = 1; row < total_rows; row++) {
        u32 *src = fb.addr + row * (FONT_CHAR_HEIGHT * pitch_dwords);
        u32 *dst = fb.addr + (row - 1) * (FONT_CHAR_HEIGHT * pitch_dwords);
        memcpy(dst, src, FONT_CHAR_HEIGHT * row_bytes);
    }

    /* 清空最后一行 */
    u32 *last_row = fb.addr + (total_rows - 1) * (FONT_CHAR_HEIGHT * pitch_dwords);
    u32 bg_pixel = color_to_pixel(console_bg);
    u32 total_cols = CONSOLE_COLS * FONT_CHAR_WIDTH;
    for (u32 row = 0; row < FONT_CHAR_HEIGHT; row++) {
        for (u32 col = 0; col < total_cols; col++) {
            last_row[row * pitch_dwords + col] = bg_pixel;
        }
    }
}

void fb_console_init(void)
{
    fb_clear(console_bg);
    console_row = 0;
    console_col = 0;
}

void fb_console_putc(int c)
{
    if (c == '\b') {
        if (console_col > 0) console_col--;
        fb_putchar(' ',
                   console_col * FONT_CHAR_WIDTH,
                   console_row * FONT_CHAR_HEIGHT,
                   console_fg, console_bg);
    } else if (c == '\n') {
        console_col = 0;
        console_row++;
    } else if (c == '\r') {
        console_col = 0;
    } else if (c == '\t') {
        int tab_stop = 4;
        console_col = (console_col + tab_stop) / tab_stop * tab_stop;
    } else {
        fb_putchar(c,
                   console_col * FONT_CHAR_WIDTH,
                   console_row * FONT_CHAR_HEIGHT,
                   console_fg, console_bg);
        console_col++;
    }

    /* 换行处理 */
    u32 max_col = CONSOLE_COLS;
    u32 max_row = CONSOLE_ROWS;
    if ((u32)console_col >= max_col) {
        console_col = 0;
        console_row++;
    }
    if ((u32)console_row >= max_row) {
        fb_console_scroll();
        console_row = (int)(max_row - 1);
    }
}

void fb_console_puts(const char *s)
{
    while (*s) {
        fb_console_putc(*s++);
    }
}

void fb_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buf[256];
    int pos = 0;

    while (*fmt && pos < 255) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *str = va_arg(args, const char *);
                    while (*str && pos < 255)
                        buf[pos++] = *str++;
                    break;
                }
                case 'd':
                case 'x': {
                    u32 n = va_arg(args, u32);
                    char tmp[16];
                    int ti = 0;
                    if (*fmt == 'x') {
                        static const char hex[] = "0123456789ABCDEF";
                        if (n == 0) {
                            tmp[ti++] = '0';
                        } else {
                            while (n > 0) {
                                tmp[ti++] = hex[n & 0xF];
                                n >>= 4;
                            }
                        }
                    } else {
                        if (n == 0) {
                            tmp[ti++] = '0';
                        } else {
                            while (n > 0) {
                                tmp[ti++] = '0' + (n % 10);
                                n /= 10;
                            }
                        }
                    }
                    /* 反转数字 */
                    for (int j = ti - 1; j >= 0; j--)
                        buf[pos++] = tmp[j];
                    break;
                }
                case 'c':
                    buf[pos++] = (char)va_arg(args, int);
                    break;
                case '%':
                    buf[pos++] = '%';
                    break;
                default:
                    buf[pos++] = '%';
                    buf[pos++] = *fmt;
                    break;
            }
        } else {
            buf[pos++] = *fmt;
        }
        fmt++;
    }
    buf[pos] = '\0';
    va_end(args);

    fb_console_puts(buf);
}
