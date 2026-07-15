#include "cursor.h"
#include "framebuffer.h"
#include "mouse.h"
#include "serial.h"

/* ── 全局光标状态 ── */
cursor_t g_cursor;

/* ── 光标精灵（箭头指针） ──
 *
 * 16×16 像素，每个像素的含义：
 *   0 = 透明（不绘制）
 *   1 = 黑色（轮廓）
 *   2 = 白色（填充）
 *
 * 热点位于 (0,0) —— 箭头尖端
 */
static const u8 s_cursor_sprite[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

/* 光标颜色 */
static color_t s_cursor_black = {0, 0, 0, 0};
static color_t s_cursor_white = {255, 255, 255, 0};

/* ── 内部函数 ── */

/* 约束坐标，使光标完全位于屏幕内 */
static void clamp_position(i32 *x, i32 *y)
{
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x + CURSOR_WIDTH > (i32)fb.width)  *x = (i32)fb.width  - CURSOR_WIDTH;
    if (*y + CURSOR_HEIGHT > (i32)fb.height) *y = (i32)fb.height - CURSOR_HEIGHT;
}

/* 将光标当前位置的背景保存到 saved_bg */
static void save_background(void)
{
    if (!fb.addr) return;
    u32 pitch_dwords = fb.pitch / 4;
    for (u32 row = 0; row < CURSOR_HEIGHT; row++) {
        for (u32 col = 0; col < CURSOR_WIDTH; col++) {
            u32 sx = (u32)g_cursor.x + col;
            u32 sy = (u32)g_cursor.y + row;
            if (sx < fb.width && sy < fb.height) {
                g_cursor.saved_bg[row][col] = fb.addr[sy * pitch_dwords + sx];
            }
        }
    }
}

/* 将保存的背景恢复到帧缓冲 */
static void restore_background(void)
{
    if (!fb.addr) return;
    u32 pitch_dwords = fb.pitch / 4;
    for (u32 row = 0; row < CURSOR_HEIGHT; row++) {
        for (u32 col = 0; col < CURSOR_WIDTH; col++) {
            u32 sx = (u32)g_cursor.old_x + col;
            u32 sy = (u32)g_cursor.old_y + row;
            if (sx < fb.width && sy < fb.height) {
                fb.addr[sy * pitch_dwords + sx] = g_cursor.saved_bg[row][col];
            }
        }
    }
}

/* 在当前位置绘制光标精灵 */
static void draw_cursor(void)
{
    if (!fb.addr) return;
    u32 pitch_dwords = fb.pitch / 4;
    u32 black_pixel = ((u32)s_cursor_black.r << 16) |
                      ((u32)s_cursor_black.g << 8) |
                      s_cursor_black.b;
    u32 white_pixel = ((u32)s_cursor_white.r << 16) |
                      ((u32)s_cursor_white.g << 8) |
                      s_cursor_white.b;

    for (u32 row = 0; row < CURSOR_HEIGHT; row++) {
        for (u32 col = 0; col < CURSOR_WIDTH; col++) {
            u8 val = s_cursor_sprite[row][col];
            if (val == 0) continue; /* 透明 */

            u32 sx = (u32)g_cursor.x + col;
            u32 sy = (u32)g_cursor.y + row;
            if (sx >= fb.width || sy >= fb.height) continue;

            u32 pixel = (val == 1) ? black_pixel : white_pixel;
            fb.addr[sy * pitch_dwords + sx] = pixel;
        }
    }
}

/* ── 公开 API ── */

void cursor_init(void)
{
    g_cursor.x      = (i32)(fb.width  / 2);
    g_cursor.y      = (i32)(fb.height / 2);
    g_cursor.old_x  = g_cursor.x;
    g_cursor.old_y  = g_cursor.y;
    g_cursor.visible = 1;

    clamp_position(&g_cursor.x, &g_cursor.y);
    g_cursor.old_x = g_cursor.x;
    g_cursor.old_y = g_cursor.y;

    if (fb.addr) {
        save_background();
        draw_cursor();
    }

    serial_printf(SERIAL_COM1, "[cursor] initialized at (%d, %d)\n",
                  g_cursor.x, g_cursor.y);
}

void cursor_show(void)
{
    if (g_cursor.visible) return;
    g_cursor.visible = 1;
    save_background();
    draw_cursor();
}

void cursor_hide(void)
{
    if (!g_cursor.visible) return;
    g_cursor.visible = 0;
    restore_background();
}

void cursor_move(i32 dx, i32 dy)
{
    if (!g_cursor.visible || !fb.addr) return;

    /* 保存旧位置（恢复背景用） */
    g_cursor.old_x = g_cursor.x;
    g_cursor.old_y = g_cursor.y;

    /* 计算新位置 */
    g_cursor.x += dx;
    g_cursor.y += dy;
    clamp_position(&g_cursor.x, &g_cursor.y);

    /* 如果位置没有变化，跳过 */
    if (g_cursor.x == g_cursor.old_x && g_cursor.y == g_cursor.old_y)
        return;

    /* 恢复旧位置的背景 */
    restore_background();

    /* 保存新位置的背景并绘制光标 */
    save_background();
    draw_cursor();
}

void cursor_poll_mouse(void)
{
    mouse_event_t ev;
    while (mouse_read(&ev)) {
        /* 每次移动累加所有待处理事件 */
        cursor_move(ev.dx, -ev.dy); /* Y 轴翻转：屏幕坐标 Y 向下为正，鼠标向下为 +dy */
    }
}

void cursor_redraw(void)
{
    if (!g_cursor.visible || !fb.addr) return;
    save_background();
    draw_cursor();
}

/* ── 合成辅助（供窗口管理器使用） ── */

/* 用保存的背景覆盖光标当前位置（移除光标像素），不改变 visible */
void cursor_remove(void)
{
    if (!fb.addr || !g_cursor.visible) return;

    u32 pitch_dwords = fb.pitch / 4;
    for (u32 row = 0; row < CURSOR_HEIGHT; row++) {
        for (u32 col = 0; col < CURSOR_WIDTH; col++) {
            u32 sx = (u32)g_cursor.x + col;
            u32 sy = (u32)g_cursor.y + row;
            if (sx < fb.width && sy < fb.height) {
                fb.addr[sy * pitch_dwords + sx] = g_cursor.saved_bg[row][col];
            }
        }
    }
}

/* 重新保存光标位置的背景并绘制光标（合成后调用），不改变 visible */
void cursor_restore(void)
{
    if (!fb.addr || !g_cursor.visible) return;

    g_cursor.old_x = g_cursor.x;
    g_cursor.old_y = g_cursor.y;
    save_background();
    draw_cursor();
}
