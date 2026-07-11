#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// TIM2 as a fixed-rate tick source for the function generator.
// timer_init sets the update rate; timer_wait_tick blocks until the next tick,
// giving jitter-free timing without needing interrupts.
void timer_init(uint32_t fs_hz);
void timer_wait_tick(void);

#endif
