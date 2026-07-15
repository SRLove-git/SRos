#include "keyboard.h"
#include "io.h"
#define KEYBOARD_BUFFER_SIZE 256
static const u8 kbd_us_normal[128] = {
    [0x01] = 0x1B,           // Esc
    [0x02] = '1',  [0x03] = '2',  [0x04] = '3',
    [0x05] = '4',  [0x06] = '5',  [0x07] = '6',
    [0x08] = '7',  [0x09] = '8',  [0x0A] = '9',
    [0x0B] = '0',  [0x0C] = '-',  [0x0D] = '=',
    [0x0E] = 0x08,            // Backspace
    [0x0F] = '\t',            // Tab
    [0x10] = 'q',  [0x11] = 'w',  [0x12] = 'e',
    [0x13] = 'r',  [0x14] = 't',  [0x15] = 'y',
    [0x16] = 'u',  [0x17] = 'i',  [0x18] = 'o',
    [0x19] = 'p',  [0x1A] = '[',  [0x1B] = ']',
    [0x1C] = '\n',            // Enter
    [0x1E] = 'a',  [0x1F] = 's',  [0x20] = 'd',
    [0x21] = 'f',  [0x22] = 'g',  [0x23] = 'h',
    [0x24] = 'j',  [0x25] = 'k',  [0x26] = 'l',
    [0x27] = ';',  [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',             // 反斜杠
    [0x2C] = 'z',  [0x2D] = 'x',  [0x2E] = 'c',
    [0x2F] = 'v',  [0x30] = 'b',  [0x31] = 'n',
    [0x32] = 'm',  [0x33] = ',',  [0x34] = '.',
    [0x35] = '/',
    [0x37] = '*',             // 小键盘乘号
    [0x39] = ' ',             // 空格
};

/* Shift 按下时 */
static const u8 kbd_us_shift[128] = {
    [0x01] = 0x1B,            // Esc
    [0x02] = '!',  [0x03] = '@',  [0x04] = '#',
    [0x05] = '$',  [0x06] = '%',  [0x07] = '^',
    [0x08] = '&',  [0x09] = '*',  [0x0A] = '(',
    [0x0B] = ')',  [0x0C] = '_',  [0x0D] = '+',
    [0x0E] = 0x08,            // Backspace
    [0x0F] = '\t',            // Tab
    [0x10] = 'Q',  [0x11] = 'W',  [0x12] = 'E',
    [0x13] = 'R',  [0x14] = 'T',  [0x15] = 'Y',
    [0x16] = 'U',  [0x17] = 'I',  [0x18] = 'O',
    [0x19] = 'P',  [0x1A] = '{',  [0x1B] = '}',
    [0x1C] = '\n',            // Enter
    [0x1E] = 'A',  [0x1F] = 'S',  [0x20] = 'D',
    [0x21] = 'F',  [0x22] = 'G',  [0x23] = 'H',
    [0x24] = 'J',  [0x25] = 'K',  [0x26] = 'L',
    [0x27] = ':',  [0x28] = '"',  [0x29] = '~',
    [0x2B] = '|',             // 反斜杠
    [0x2C] = 'Z',  [0x2D] = 'X',  [0x2E] = 'C',
    [0x2F] = 'V',  [0x30] = 'B',  [0x31] = 'N',
    [0x32] = 'M',  [0x33] = '<',  [0x34] = '>',
    [0x35] = '?',
    [0x37] = '*',             // 小键盘乘号
    [0x39] = ' ',             // 空格
};

static u8 shift_pressed = 0;
static u8 ctrl_pressed = 0;
static u8 caps_lock_active = 0;


static unsigned char kbd_buffer[KEYBOARD_BUFFER_SIZE];
static int  kbd_buffer_head = 0;  // 写指针
static int  kbd_buffer_tail = 0;  // 读指针

// 向缓冲区写入一个字符（在中断处理函数中调用）
void kbd_buffer_put(int c) {
    int next = (kbd_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != kbd_buffer_tail) {          // 检查缓冲区是否已满
        kbd_buffer[kbd_buffer_head] = (unsigned char)c;
        kbd_buffer_head = next;
    }
    // 如果满了，丢弃该字符（防止中断处理函数阻塞）
}

// 从缓冲区读取一个字符（在主循环中调用）
int kbd_buffer_get(void) {
    if (kbd_buffer_head == kbd_buffer_tail) {
        return 0;  // 缓冲区为空
    }
    int c = kbd_buffer[kbd_buffer_tail];
    kbd_buffer_tail = (kbd_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

// 检查缓冲区是否有数据
int kbd_buffer_has_data(void) {
    return kbd_buffer_head != kbd_buffer_tail;
}
void keyboard_handler(registers_t *regs){
    (void)regs;
    u8 scancode = inb(0x60);
    /* 松开事件 */
    if(scancode & 0x80){
        u8 code = scancode & 0x7F; //去掉释放位
        if(code == 0x2A || code == 0x36){
            shift_pressed = 0;
        }else if(code == 0x1D) //Ctrl
            ctrl_pressed = 0;
        return;
    }
    switch(scancode){
        case 0x2A:
        case 0x36:
                shift_pressed = 1;return;
        case 0x1D:
                ctrl_pressed = 1; return;
        case 0x3A:
            caps_lock_active = !caps_lock_active;
            return;
    }
    /* 查表翻译 */
    u8 ch = shift_pressed ? kbd_us_shift[scancode] 
                          : kbd_us_normal[scancode];

    /* 方向键不在布局表中，单独处理 */
    if (ch == 0) {
        if (scancode == 0x48) { kbd_buffer_put(KEY_UP); return; }
        if (scancode == 0x50) { kbd_buffer_put(KEY_DOWN); return; }
        if (scancode == 0x4B) { kbd_buffer_put(KEY_LEFT); return; }
        if (scancode == 0x4D) { kbd_buffer_put(KEY_RIGHT); return; }
    }

    if(caps_lock_active && ch >= 'a' && ch <= 'z'){
        ch -= 32;
    }else if(caps_lock_active && ch >= 'A' && ch <= 'Z'){
        ch += 32;
    }
    if(ch != 0 ){
        kbd_buffer_put(ch);
    }
}