#ifndef TIMER_H
#define TIMER_H

#include <lib.h>

void timer_init(u32 interval_ms);
void timer_handler();
u64 timer_get_frq();

#endif