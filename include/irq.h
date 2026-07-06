#ifndef IRQ_H
#define IRQ_H
#include "types.h"
#include "idt.h"
extern volatile u32 timer_ticks;
void pit_init(u32 frequency_hz);
void timer_handler(registers_t *regs);
void pic_remap();
void irq_handler(registers_t *regs);

#endif /* IRQ_H */