#include "window.h"
#include "cursor.h"
#include "mouse.h"
#include "vgafont.h"
#include "kmalloc.h"
#include "serial.h"
#include "string.h"

/* ── 全局窗口管理器状态 ── */
wm_t g_wm;

/* 上一次鼠标按键状态（用于去抖） */
static u8 wm_buttons_last = 0;

/* ── 颜色转换辅助 ── */
static inline u32 color_to_pixel(color_t c)
{
    return ((u32)c.r << 16) | ((u32)c.g << 8) | (u32)c.b;
}

/* ================================================================
 *  内部函数：链表操作
 * ================================================================ */

/* 将窗口按 z_order 升序插入链表 */
static void insert_sorted(window_t *win)
{
    if (!g_wm.windows || win->z_order < g_wm.windows->z_order) {
        win->next = g_wm.windows;
        g_wm.windows = win;
        return;
    }

    window_t *prev = g_wm.windows;
    while (prev->next && prev->next->z_order <= win->z_order)
        prev = prev->next;

    win->next = prev->next;
    prev->next = win;
}

/* 从链表移除窗口（按指针查找） */
static void remove_from_list(window_t *win)
{
    if (!g_wm.windows) return;

    if (g_wm.windows == win) {
        g_wm.windows = win->next;
        return;
    }

    window_t *prev = g_wm.windows;
    while (prev->next) {
        if (prev->next == win) {
            prev->next = win->next;
            return;
        }
        prev = prev->next;
    }
}

/* 获取最高的 z_order 值 */
static u32 get_next_z_order(void)
{
    u32 max_z = 0;
    window_t *w = g_wm.windows;
    while (w) {
        if (w->z_order > max_z) max_z = w->z_order;
        w = w->next;
    }
    return max_z + 1;
}

/* 计算窗口总尺寸（含边框和标题栏） */
static void compute_window_size(window_t *win)
{
    win->width  = win->content_w + WM_BORDER_WIDTH * 2;
    win->height = win->content_h + WM_TITLE_HEIGHT + WM_BORDER_WIDTH;
}

/* ================================================================
 *  内部函数：绘制单个窗口的框架（边框 + 标题栏 + 关闭按钮）
 * ================================================================ */

static void draw_window_frame(window_t *win)
{
    u32 pitch_dwords = fb.pitch / 4;
    u32 fg_border, fg_title;

    if (win->focused) {
        fg_border = color_to_pixel(WM_BORDER_ACTIVE);
        fg_title  = color_to_pixel(WM_TITLE_ACTIVE);
    } else {
        fg_border = color_to_pixel(WM_BORDER_INACTIVE);
        fg_title  = color_to_pixel(WM_TITLE_INACTIVE);
    }

    u32 title_text = color_to_pixel(WM_TITLE_TEXT);
    u32 close_pixel = color_to_pixel(WM_CLOSE_NORMAL);
    u32 close_x_pixel = color_to_pixel(WM_CLOSE_X_COLOR);

    /* 窗口边界裁剪 */
    i32 wx = win->x, wy = win->y;
    u32 ww = win->width, wh = win->height;

    /* ── 绘制边框（2 像素宽） ── */
    for (u32 b = 0; b < WM_BORDER_WIDTH; b++) {
        /* 上边框 */
        for (u32 cx = 0; cx < ww; cx++) {
            u32 sx = (u32)(wx + cx);
            u32 sy = (u32)(wy + b);
            if (sx < fb.width && sy < fb.height)
                fb.addr[sy * pitch_dwords + sx] = fg_border;
        }
        /* 下边框 */
        for (u32 cx = 0; cx < ww; cx++) {
            u32 sx = (u32)(wx + cx);
            u32 sy = (u32)(wy + wh - 1 - b);
            if (sx < fb.width && sy < fb.height)
                fb.addr[sy * pitch_dwords + sx] = fg_border;
        }
        /* 左边框 */
        for (u32 cy = 0; cy < wh; cy++) {
            u32 sx = (u32)(wx + b);
            u32 sy = (u32)(wy + cy);
            if (sx < fb.width && sy < fb.height)
                fb.addr[sy * pitch_dwords + sx] = fg_border;
        }
        /* 右边框 */
        for (u32 cy = 0; cy < wh; cy++) {
            u32 sx = (u32)(wx + ww - 1 - b);
            u32 sy = (u32)(wy + cy);
            if (sx < fb.width && sy < fb.height)
                fb.addr[sy * pitch_dwords + sx] = fg_border;
        }
    }

    /* ── 标题栏 ── */
    i32 title_y = wy + WM_BORDER_WIDTH;
    for (u32 ty = 0; ty < WM_TITLE_HEIGHT; ty++) {
        for (u32 tx = WM_BORDER_WIDTH; tx < ww - WM_BORDER_WIDTH; tx++) {
            u32 sx = (u32)(wx + tx);
            u32 sy = (u32)(title_y + ty);
            if (sx < fb.width && sy < fb.height)
                fb.addr[sy * pitch_dwords + sx] = fg_title;
        }
    }

    /* ── 标题文字（居中） ── */
    if (win->title[0]) {
        u32 text_len = strlen(win->title);
        i32 text_x = wx + WM_BORDER_WIDTH + 4;
        /* 标题栏可显示的字符数 */
        u32 max_title_chars = (ww - WM_BORDER_WIDTH * 2 - 4 - WM_CLOSE_BTN_W - WM_CLOSE_BTN_MARG * 2) / FONT_CHAR_WIDTH;

        for (u32 ci = 0; ci < text_len && ci < max_title_chars; ci++) {
            int ch = (unsigned char)win->title[ci];
            if (ch < FONT_FIRST_CHAR || ch >= FONT_FIRST_CHAR + FONT_NUM_CHARS)
                ch = ' ';
            int idx = ch - FONT_FIRST_CHAR;
            i32 cx = text_x + ci * FONT_CHAR_WIDTH;
            i32 cy = title_y + (WM_TITLE_HEIGHT - FONT_CHAR_HEIGHT) / 2;

            for (u32 row = 0; row < FONT_CHAR_HEIGHT; row++) {
                u8 bits = font_vga8x16[idx][row];
                for (u32 col = 0; col < FONT_CHAR_WIDTH; col++) {
                    u32 sx = (u32)(cx + col);
                    u32 sy = (u32)(cy + row);
                    if (sx < fb.width && sy < fb.height) {
                        fb.addr[sy * pitch_dwords + sx] =
                            (bits & (0x80 >> col)) ? title_text : fg_title;
                    }
                }
            }
        }
    }

    /* ── 关闭按钮（右上角红色方块 + X） ── */
    i32 btn_x = wx + (i32)ww - WM_BORDER_WIDTH - WM_CLOSE_BTN_MARG - WM_CLOSE_BTN_W;
    i32 btn_y = title_y + (WM_TITLE_HEIGHT - WM_CLOSE_BTN_H) / 2;

    /* 按钮背景 */
    for (u32 by = 0; by < WM_CLOSE_BTN_H; by++) {
        for (u32 bx = 0; bx < WM_CLOSE_BTN_W; bx++) {
            u32 sx = (u32)(btn_x + bx);
            u32 sy = (u32)(btn_y + by);
            if (sx < fb.width && sy < fb.height)
                fb.addr[sy * pitch_dwords + sx] = close_pixel;
        }
    }

    /* 绘制白色 X（两条对角线） */
    for (u32 i = 0; i < WM_CLOSE_BTN_W; i++) {
        u32 x1 = (u32)(btn_x + i);
        u32 y1 = (u32)(btn_y + i);
        if (x1 < fb.width && y1 < fb.height)
            fb.addr[y1 * pitch_dwords + x1] = close_x_pixel;

        u32 x2 = (u32)(btn_x + i);
        u32 y2 = (u32)(btn_y + WM_CLOSE_BTN_H - 1 - i);
        if (x2 < fb.width && y2 < fb.height)
            fb.addr[y2 * pitch_dwords + x2] = close_x_pixel;
    }
}

/* 绘制窗口的内容区域（从窗口缓冲 blit 到帧缓冲） */
static void draw_window_content(window_t *win)
{
    if (!win->buffer) return;

    u32 pitch_dwords = fb.pitch / 4;

    i32 content_x = win->x + WM_BORDER_WIDTH;
    i32 content_y = win->y + WM_BORDER_WIDTH + WM_TITLE_HEIGHT;

    for (u32 ry = 0; ry < win->content_h; ry++) {
        for (u32 rx = 0; rx < win->content_w; rx++) {
            u32 sx = (u32)(content_x + rx);
            u32 sy = (u32)(content_y + ry);
            if (sx >= fb.width || sy >= fb.height) continue;
            fb.addr[sy * pitch_dwords + sx] = win->buffer[ry * win->content_w + rx];
        }
    }
}

/* ================================================================
 *  内部函数：检测窗口是否包含某一点
 * ================================================================ */
static int window_contains(window_t *win, i32 px, i32 py)
{
    return (px >= win->x && px < win->x + (i32)win->width &&
            py >= win->y && py < win->y + (i32)win->height);
}

/* ================================================================
 *  公开 API
 * ================================================================ */

/* ── 初始化 ── */
void wm_init(void)
{
    g_wm.windows   = NULL;
    g_wm.focused   = NULL;
    g_wm.count     = 0;
    g_wm.needs_composite = 1;
    g_wm.desktop_color = WM_DESKTOP_COLOR;

    serial_printf(SERIAL_COM1, "[wm] window manager initialized (%dx%d)\n",
                  fb.width, fb.height);
}

/* ── 创建窗口 ── */
window_t *wm_create_window(i32 x, i32 y, u32 content_w, u32 content_h,
                           const char *title)
{
    if (g_wm.count >= WM_MAX_WINDOWS) return NULL;

    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) return NULL;

    win->x         = x;
    win->y         = y;
    win->content_w = content_w;
    win->content_h = content_h;
    win->visible   = 1;
    win->focused   = 0;
    win->dirty     = 1;
    win->next      = NULL;
    win->buffer    = NULL;

    /* 复制标题 */
    int i;
    for (i = 0; title[i] && i < WM_MAX_TITLE_LEN; i++)
        win->title[i] = title[i];
    win->title[i] = '\0';

    /* 计算总尺寸 */
    compute_window_size(win);

    /* 分配内容区像素缓冲 */
    win->buffer = (u32 *)kmalloc(content_w * content_h * sizeof(u32));
    if (!win->buffer) {
        kfree(win);
        return NULL;
    }

    /* 用背景色初始化缓冲 */
    u32 bg = color_to_pixel(WM_CONTENT_BG);
    for (u32 i = 0; i < content_w * content_h; i++)
        win->buffer[i] = bg;

    /* 设置 z_order 并插入链表 */
    win->z_order = get_next_z_order();
    insert_sorted(win);
    g_wm.count++;

    /* 设为焦点 */
    wm_focus_window(win);
    g_wm.needs_composite = 1;

    serial_printf(SERIAL_COM1, "[wm] created window '%s' at (%d,%d) %dx%d z=%u\n",
                  title, x, y, win->width, win->height, win->z_order);

    return win;
}

/* ── 销毁窗口 ── */
void wm_destroy_window(window_t *win)
{
    if (!win) return;

    remove_from_list(win);

    if (g_wm.focused == win)
        g_wm.focused = NULL;

    if (win->buffer) {
        kfree(win->buffer);
        win->buffer = NULL;
    }

    g_wm.count--;
    g_wm.needs_composite = 1;

    serial_printf(SERIAL_COM1, "[wm] destroyed window '%s'\n", win->title);
    kfree(win);
}

/* ── 移动窗口 ── */
void wm_move_window(window_t *win, i32 x, i32 y)
{
    if (!win) return;
    win->x = x;
    win->y = y;
    win->dirty = 1;
    g_wm.needs_composite = 1;
}

/* ── 聚焦窗口 ── */
void wm_focus_window(window_t *win)
{
    if (!win) return;

    /* 如果已经是聚焦窗口，只更新 z_order */
    if (g_wm.focused == win) {
        win->focused = 1;
        return;
    }

    /* 取消旧焦点 */
    if (g_wm.focused) {
        g_wm.focused->focused = 0;
    }

    /* 设置新焦点 */
    win->focused = 1;
    g_wm.focused = win;

    /* 提升 z_order 到最高 */
    u32 new_z = get_next_z_order();
    remove_from_list(win);
    win->z_order = new_z;
    insert_sorted(win);

    win->dirty = 1;
    g_wm.needs_composite = 1;
}

/* ── 合成所有窗口到帧缓冲 ── */
void wm_composite(void)
{
    if (!fb.addr) return;
    if (g_wm.count == 0 && !g_wm.needs_composite) return;

    /* 0. 合成前移除光标 */
    int cursor_was_visible = g_cursor.visible;
    if (cursor_was_visible)
        cursor_remove();

    /* ★ 没有任何窗口 → 清空屏幕背景（使用 rep stosd，QEMU TCG 会优化为 memset） */
    if (g_wm.count == 0) {
        u32 bg = color_to_pixel(g_wm.desktop_color);
        u32 pitch_dwords = fb.pitch / 4;
        u32 count = fb.height * pitch_dwords;
        __asm__ volatile (
            "cld\n"
            "rep stosl\n"
            : "+D" (fb.addr), "+c" (count)
            : "a" (bg)
            : "memory"
        );

        if (cursor_was_visible)
            cursor_restore();
        g_wm.needs_composite = 0;
        return;
    }

    /* 1. 按 Z 序（从低到高）绘制每个可见窗口
     *
     * 注意：常规合成不做全屏清空操作！
     * 在 QEMU TCG 模式下，786K 像素的填充即使使用 rep stosd 也极慢，
     * 会导致定时器 IRQ 阻塞过久，表现为鼠标卡死。
     *
     * 窗口直接绘制在现有帧缓冲内容之上，窗口关闭时的残留由上面
     * g_wm.count == 0 分支处理。
     */
    window_t *w = g_wm.windows;
    while (w) {
        if (w->visible) {
            draw_window_frame(w);
            draw_window_content(w);
            w->dirty = 0;
        }
        w = w->next;
    }

    /* 3. 恢复光标 */
    if (cursor_was_visible)
        cursor_restore();

    g_wm.needs_composite = 0;
}

/* ── 获取 (x, y) 处的窗口（Z 序从高到低） ── */
window_t *wm_get_window_at(i32 x, i32 y)
{
    /* 遍历链表（升序 Z 序），最后匹配的即为 Z 序最高的窗口 */
    window_t *result = NULL;
    window_t *w = g_wm.windows;
    while (w) {
        if (w->visible && window_contains(w, x, y))
            result = w;
        w = w->next;
    }
    return result;
}

/* ── 判断是否在关闭按钮上 ── */
int wm_is_in_close_btn(window_t *win, i32 x, i32 y)
{
    if (!win) return 0;

    i32 btn_x = win->x + (i32)win->width - WM_BORDER_WIDTH - WM_CLOSE_BTN_MARG - WM_CLOSE_BTN_W;
    i32 btn_y = win->y + WM_BORDER_WIDTH + (WM_TITLE_HEIGHT - WM_CLOSE_BTN_H) / 2;

    return (x >= btn_x && x < btn_x + (i32)WM_CLOSE_BTN_W &&
            y >= btn_y && y < btn_y + (i32)WM_CLOSE_BTN_H);
}

/* ── 获取标题栏矩形 ── */
void wm_get_titlebar_rect(window_t *win, i32 *tx, i32 *ty, u32 *tw, u32 *th)
{
    if (tx) *tx = win->x + WM_BORDER_WIDTH;
    if (ty) *ty = win->y + WM_BORDER_WIDTH;
    if (tw) *tw = win->width - WM_BORDER_WIDTH * 2;
    if (th) *th = WM_TITLE_HEIGHT;
}

/* ── 处理鼠标点击 ── */
int wm_handle_click(i32 x, i32 y, u8 buttons)
{
    if (!(buttons & MOUSE_LEFT_BTN)) return 0;

    window_t *win = wm_get_window_at(x, y);
    if (!win) return 0;

    /* 检查是否点击关闭按钮 */
    if (wm_is_in_close_btn(win, x, y)) {
        wm_destroy_window(win);
        wm_composite();
        return 1;
    }

    /* 聚焦窗口 */
    wm_focus_window(win);
    wm_composite();

    return 1;
}

/* ── 轮询鼠标并更新 ── */
void wm_update(void)
{
    mouse_event_t ev;
    int click_handled = 0;

    while (mouse_read(&ev)) {
        cursor_move(ev.dx, -ev.dy);

        /* 仅在新按下左键时处理点击（去抖：避免长按重复处理） */
        u8 new_buttons = ev.buttons & MOUSE_LEFT_BTN;
        u8 prev_buttons = wm_buttons_last & MOUSE_LEFT_BTN;

        if (new_buttons && !prev_buttons && !click_handled) {
            wm_handle_click(g_cursor.x, g_cursor.y, ev.buttons);
            click_handled = 1;
        }

        wm_buttons_last = ev.buttons;
    }

    if (click_handled) {
        /* 已由 wm_handle_click 处理 */
    } else if (g_wm.needs_composite) {
        wm_composite();
    }
}

/* ================================================================
 *  窗口缓冲绘制工具
 * ================================================================ */

void wm_fill_rect(window_t *win, u32 rx, u32 ry, u32 rw, u32 rh, color_t c)
{
    if (!win || !win->buffer) return;

    /* 裁剪到内容区 */
    if (rx >= win->content_w || ry >= win->content_h) return;
    if (rx + rw > win->content_w) rw = win->content_w - rx;
    if (ry + rh > win->content_h) rh = win->content_h - ry;

    u32 pixel = color_to_pixel(c);
    for (u32 row = 0; row < rh; row++) {
        for (u32 col = 0; col < rw; col++) {
            win->buffer[(ry + row) * win->content_w + (rx + col)] = pixel;
        }
    }

    win->dirty = 1;
    g_wm.needs_composite = 1;
}

void wm_putpixel(window_t *win, u32 px, u32 py, color_t c)
{
    if (!win || !win->buffer) return;
    if (px >= win->content_w || py >= win->content_h) return;

    win->buffer[py * win->content_w + px] = color_to_pixel(c);
    win->dirty = 1;
    g_wm.needs_composite = 1;
}

void wm_putchar(window_t *win, int ch, u32 px, u32 py, color_t fg, color_t bg)
{
    if (!win || !win->buffer) return;
    if (px + FONT_CHAR_WIDTH > win->content_w) return;
    if (py + FONT_CHAR_HEIGHT > win->content_h) return;

    if (ch < FONT_FIRST_CHAR || ch >= FONT_FIRST_CHAR + FONT_NUM_CHARS)
        ch = ' ';
    int idx = ch - FONT_FIRST_CHAR;

    u32 fg_pixel = color_to_pixel(fg);
    u32 bg_pixel = color_to_pixel(bg);

    for (u32 row = 0; row < FONT_CHAR_HEIGHT; row++) {
        u8 bits = font_vga8x16[idx][row];
        for (u32 col = 0; col < FONT_CHAR_WIDTH; col++) {
            u32 pixel = (bits & (0x80 >> col)) ? fg_pixel : bg_pixel;
            win->buffer[(py + row) * win->content_w + (px + col)] = pixel;
        }
    }

    win->dirty = 1;
    g_wm.needs_composite = 1;
}

void wm_puts(window_t *win, const char *s, u32 px, u32 py, color_t fg, color_t bg)
{
    if (!win || !s) return;

    u32 cx = px;
    u32 cy = py;
    while (*s) {
        if (*s == '\n') {
            cx = px;
            cy += FONT_CHAR_HEIGHT;
        } else {
            wm_putchar(win, *s, cx, cy, fg, bg);
            cx += FONT_CHAR_WIDTH;
            if (cx + FONT_CHAR_WIDTH > win->content_w) {
                cx = px;
                cy += FONT_CHAR_HEIGHT;
            }
        }
        s++;
    }
}

void wm_printf(window_t *win, u32 px, u32 py, color_t fg, color_t bg,
               const char *fmt, ...)
{
    if (!win) return;

    char buf[256];
    int pos = 0;

    /* 简易 vargs 处理 */
    u32 *args = (u32 *)(&fmt + 1);

    while (*fmt && pos < 255) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *str = (const char *)*args++;
                    while (*str && pos < 255)
                        buf[pos++] = *str++;
                    break;
                }
                case 'd':
                case 'u': {
                    u32 n = *args++;
                    char tmp[16];
                    int ti = 0;
                    if (n == 0) {
                        tmp[ti++] = '0';
                    } else {
                        while (n > 0) {
                            tmp[ti++] = '0' + (n % 10);
                            n /= 10;
                        }
                    }
                    for (int j = ti - 1; j >= 0; j--)
                        buf[pos++] = tmp[j];
                    break;
                }
                case 'x': {
                    u32 n = *args++;
                    char tmp[16];
                    int ti = 0;
                    static const char hex[] = "0123456789abcdef";
                    if (n == 0) {
                        tmp[ti++] = '0';
                    } else {
                        while (n > 0) {
                            tmp[ti++] = hex[n & 0xF];
                            n >>= 4;
                        }
                    }
                    for (int j = ti - 1; j >= 0; j--)
                        buf[pos++] = tmp[j];
                    break;
                }
                case 'c': {
                    buf[pos++] = (char)*args++;
                    break;
                }
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

    wm_puts(win, buf, px, py, fg, bg);
}
