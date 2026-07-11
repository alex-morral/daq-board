#include "timer.h"

// TIM2 (general-purpose timer) on APB1. With the chip running on the 8 MHz HSI
// and the default prescalers, the timer counter is clocked at 8 MHz.
#define TIM_CLK_HZ      8000000UL

#define RCC_BASE        0x40021000
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1C))

#define TIM2_BASE       0x40000000
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00))
#define TIM2_SR         (*(volatile uint32_t *)(TIM2_BASE + 0x10))
#define TIM2_EGR        (*(volatile uint32_t *)(TIM2_BASE + 0x14))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2C))

void timer_init(uint32_t fs_hz) {
    RCC_APB1ENR |= (1 << 0);              // enable TIM2 clock

    // Counter ticks at TIM_CLK_HZ; it rolls over (and sets the update flag)
    // every ARR+1 counts. Choose ARR so that happens fs_hz times per second.
    TIM2_PSC = 0;                          // no prescale → full 8 MHz
    TIM2_ARR = (TIM_CLK_HZ / fs_hz) - 1;

    TIM2_EGR = (1 << 0);                   // UG: load PSC/ARR immediately
    TIM2_SR  = 0;                          // clear any pending update flag
    TIM2_CR1 = (1 << 0);                   // CEN: start the timer
}

void timer_wait_tick(void) {
    while (!(TIM2_SR & (1 << 0)));         // wait for UIF (update / roll-over)
    TIM2_SR &= ~(1 << 0);                  // clear it for the next tick
}
