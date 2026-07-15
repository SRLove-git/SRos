#ifndef CURSOR_H
#define CURSOR_H

#include "types.h"

/* 鼠标光标尺寸 */
#define CURSOR_WIDTH  16
#define CURSOR_HEIGHT 16

/* 光标状态 */
typedef struct {
    i32  x, y;         /* 当前绝对位置（左上角） */
    i32  old_x, old_y; /* 上一帧位置 */
    u8   visible;      /* 是否可见 */
    u32  saved_bg[CURSOR_HEIGHT][CURSOR_WIDTH]; /* 保存的背景像素 */
} cursor_t;

extern cursor_t g_cursor;

/* ── API ── */

/* 初始化光标（放在屏幕中央） */
void cursor_init(void);

/* 设置光标可见性 */
void cursor_show(void);
void cursor_hide(void);

/* 根据鼠标位移更新光标位置并重绘 */
void cursor_move(i32 dx, i32 dy);

/* 从鼠标环形缓冲区读取所有事件并更新光标 */
void cursor_poll_mouse(void);

/* 强制重绘光标（例如在控制台滚动后调用） */
void cursor_redraw(void);

/* ── 合成辅助（供窗口管理器使用） ── */

/* 从帧缓冲移除光标（用保存的背景覆盖当前位置），不修改 visible 标志 */
void cursor_remove(void);

/* 重新保存当前位置的背景并绘制光标（合成后调用），不修改 visible 标志 */
void cursor_restore(void);

#endif /* CURSOR_H */
