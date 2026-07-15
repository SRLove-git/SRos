#include "vga.h"
#include "stdarg.h"
#include "io.h"
#include "serial.h"
#include "keyboard.h"
// vga_buffer[行 * 80 + 列] = (属性 << 8) | 字符
#define VGA_ADDR   0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define WHITE_ON_BLACK  0x0F  // 白字黑底（最常用）
#define RED_ON_BLACK    0x04  // 红字黑底
#define GREEN_ON_BLACK  0x02  // 绿字黑底
#define WHITE_ON_BLUE   0x1F  // 白字蓝底
#define YELLOW_ON_BLACK 0x0E  // 黄字黑底
int cursor_row = 0;
int cursor_col = 0;

static unsigned short *const vga_buffer = (unsigned short *)VGA_ADDR;

void vga_init(void){
    int row,col;
    for(row = 0;row < VGA_HEIGHT;row++){
        for(col = 0;col < VGA_WIDTH;col++){
            vga_putchar_at(' ',WHITE_ON_BLACK,row,col);
        }
    }
    cursor_col = 0;
    cursor_row = 0;
} 

void vga_putchar_at(char c, u8 color, int row, int col)
{
    vga_buffer[row * VGA_WIDTH + col] = (color << 8) | c;
}

void vga_putc(char c){
    if(c== '\b'){
        cursor_col--;
        vga_putchar_at(' ',WHITE_ON_BLACK,cursor_row,cursor_col);
    }else if(c == '\n'){
        cursor_col=0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) {
            vga_scroll();
            cursor_row = VGA_HEIGHT - 1;
        }
    }else if (c == CURSOR_LEFT) {
        if (cursor_col > 0) cursor_col--;
    }else if (c == CURSOR_RIGHT) {
        if (cursor_col < VGA_WIDTH - 1) cursor_col++;
    }else{
        if(cursor_col == VGA_WIDTH){
            cursor_col=0;
            cursor_row++;
        vga_scroll();
        }
        vga_putchar_at(c,WHITE_ON_BLACK,cursor_row,cursor_col);
        cursor_col++;
        if(cursor_col == VGA_WIDTH){
            cursor_col=0;
            cursor_row++;
        }
    }
    vga_set_cursor(cursor_row, cursor_col);
    
}
void vga_puts(const char *s){
    while(*s){
        vga_putc(*s++);
    }
}
void vga_scroll(){
    int row,col;
    for(row = 1;row < VGA_HEIGHT;row++){
        for(col = 0;col < VGA_WIDTH;col++){
            vga_buffer[(row-1)*VGA_WIDTH + col] = vga_buffer[row* VGA_WIDTH + col];
        }
    }
    for(col = 0;col < VGA_WIDTH;col++){
        vga_buffer[(VGA_HEIGHT - 1)*VGA_WIDTH + col] = (WHITE_ON_BLACK << 8) | ' ';
    }
}
void vga_puthex(u32 n){
 static const char hex[] = "0123456789ABCDEF";
    int i;
    vga_puts("0x");
    for (i = 28; i >= 0; i -= 4)
        vga_putc(hex[(n >> i) & 0xF]);
}

void vga_set_cursor(int row, int col)
{
    unsigned short pos = row * VGA_WIDTH + col;

    outb(0x3D4, 0x0F);          // 选择光标位置低字节寄存器
    outb(0x3D5, (u8)(pos & 0xFF));    // 写入低字节
    outb(0x3D4, 0x0E);          // 选择光标位置高字节寄存器
    outb(0x3D5, (u8)((pos >> 8) & 0xFF)); // 写入高字节
}
void vga_printf(const char *fmt, ...){
    va_list args;
    va_start(args,fmt);

    while(*fmt){
        if(*fmt == '%'){
            fmt++;
            switch(*fmt){
                case 's':
                    vga_puts(va_arg(args,const char*));
                    break;
                case 'd':
                    vga_puthex(va_arg(args,u32));
                    break;
                case 'x':
                    vga_puthex(va_arg(args,u32));
                    break;
                case 'c':
                    vga_putc(va_arg(args,int));
                    break;
                case '%':
                    vga_putc('%');
                    break;
                default:
                    vga_putc('%');
                    vga_putc(*fmt);
                    break;
            }
        }else{
            vga_putc(*fmt);
        }
        fmt++;
    }
    va_end(args);
}
void panic(const char *msg) {
    vga_puts("KERNEL PANIC: ");
    vga_puts(msg);
    serial_printf(SERIAL_COM1, "KERNEL PANIC: %s\n", msg);
    while(1);
}
