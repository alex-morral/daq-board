#ifndef HSADC_H
#define HSADC_H

#include <stdint.h>

// High-speed sampling of the STM32's internal 12-bit ADC on PA0 (broken out on
// the J2 header). A hardware timer (TIM3) triggers each conversion and DMA moves
// the result straight to RAM — the CPU does nothing during the capture, so the
// sample rate is set purely by the timer (up to ~100 kSPS).
void hsadc_init(void);

// Capture `n` samples at `rate_hz` into `buf` (blocking until DMA completes).
void hsadc_capture(uint16_t *buf, int n, uint32_t rate_hz);

#endif
