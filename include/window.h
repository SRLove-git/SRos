#ifndef WINDOW_H
#define WINDOW_H

#include "types.h"
#include "framebuffer.h"

/* ── 窗口约束 ── */
#define WM_MAX_WINDOWS     16
#define WM_TITLE_HEIGHT    20
#define WM_BORDER_WIDTH     2
#define WM_CLOSE_BTN_W     14
#define WM_CLOSE_BTN_H     14
#define WM_CLOSE_BTN_MARG   3   /* 关闭按钮到标题栏右侧的边距 */
#define WM_MAX_TITLE_LEN   31

/* ── 默认配色 ── */
#define WM_DESKTOP_COLOR   ((color_t){40, 40, 80, 0})    /* 深蓝灰桌面 */
#define WM_TITLE_ACTIVE    ((color_t){50, 80, 200, 0})   /* 活跃窗口标题栏 */
#define WM_TITLE_INACTIVE  ((color_t){100, 100, 120, 0}) /* 非活跃窗口标题栏 */
#define WM_CLOSE_NORMAL    ((color_t){200, 50, 50, 0})   /* 关闭按钮颜色 */
#define WM_CLOSE_HOVER     ((color_t){255, 80, 80, 0})   /* 悬停关闭按钮 */
#define WM_CLOSE_X_COLOR   ((color_t){255, 255, 255, 0}) /* 关闭按钮 X 颜色 */
#define WM_BORDER_ACTIVE   ((color_t){80, 110, 230, 0})  /* 活跃窗口边框 */
#define WM_BORDER_INACTIVE ((color_t){130, 130, 140, 0}) /* 非活跃窗口边框 */
#define WM_CONTENT_BG      ((color_t){245, 245, 245, 0}) /* 内容区背景 */
#define WM_TITLE_TEXT      ((color_t){255, 255, 255, 0}) /* 标题文字颜色 */

/* ── 窗口结构 ── */
typedef struct window {
    i32    x, y;             /* 窗口左上角（含边框）在屏幕上的位置 */
    u32    width, height;    /* 窗口总尺寸（含边框和标题栏） */
    u32    content_w;        /* 内容区宽度 */
    u32    content_h;        /* 内容区高度 */
    char   title[WM_MAX_TITLE_LEN + 1];
    u32    z_order;          /* Z 序（越大越靠上） */
    int    visible;
    int    focused;
    u32   *buffer;           /* 内容区像素缓冲 (content_w * content_h) */
    int    dirty;            /* 是否需要重绘 */
    struct window *next;     /* 链表指针 */
} window_t;

/* ── 窗口管理器状态 ── */
typedef struct {
    window_t *windows;       /* 按 Z 序排列的窗口链表（从低到高） */
    window_t *focused;       /* 当前聚焦窗口 */
    int       count;
    int       needs_composite;
    color_t   desktop_color;
} wm_t;

extern wm_t g_wm;

/* ── 窗口管理器 API ── */

/* 初始化窗口管理器 */
void wm_init(void);

/* 创建窗口，返回窗口指针。content_w/content_h 是内容区尺寸 */
window_t *wm_create_window(i32 x, i32 y, u32 content_w, u32 content_h,
                           const char *title);

/* 销毁窗口 */
void wm_destroy_window(window_t *win);

/* 移动窗口到新位置 */
void wm_move_window(window_t *win, i32 x, i32 y);

/* 聚焦窗口（提升到最前） */
void wm_focus_window(window_t *win);

/* 合成所有窗口到帧缓冲（+ 光标处理） */
void wm_composite(void);

/* 获取 (x, y) 处的窗口 */
window_t *wm_get_window_at(i32 x, i32 y);

/* 判断点是否在关闭按钮上 */
int wm_is_in_close_btn(window_t *win, i32 x, i32 y);

/* 处理鼠标点击，返回 1 表示已消费 */
int wm_handle_click(i32 x, i32 y, u8 buttons);

/* 获取窗口标题栏矩形（屏幕坐标） */
void wm_get_titlebar_rect(window_t *win, i32 *tx, i32 *ty,
                          u32 *tw, u32 *th);

/* ── 窗口缓冲绘制工具 ── */

/* 在窗口内容区填充矩形 */
void wm_fill_rect(window_t *win, u32 rx, u32 ry, u32 rw, u32 rh, color_t c);

/* 在窗口内容区画像素 */
void wm_putpixel(window_t *win, u32 px, u32 py, color_t c);

/* 在窗口内容区画字符 */
void wm_putchar(window_t *win, int ch, u32 px, u32 py,
                color_t fg, color_t bg);

/* 在窗口内容区输出字符串 */
void wm_puts(window_t *win, const char *s, u32 px, u32 py,
             color_t fg, color_t bg);

/* 在窗口内容区 printf */
void wm_printf(window_t *win, u32 px, u32 py, color_t fg, color_t bg,
               const char *fmt, ...);

/* ── 主循环 ── */

/* 轮询鼠标并重绘（由 mouse IRQ 或 idle 循环调用） */
void wm_update(void);

#endif /* WINDOW_H */
