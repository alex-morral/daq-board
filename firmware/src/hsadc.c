#include "hsadc.h"

// Timer counter clock (TIM3 on APB1, 8 MHz with the default prescalers).
#define TIM_CLK 8000000UL

#define RCC_BASE      0x40021000
#define RCC_AHBENR    (*(volatile uint32_t *)(RCC_BASE + 0x14))
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define RCC_APB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x1C))

#define GPIOA_BASE    0x40010800
#define GPIOA_CRL     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))

#define ADC1_BASE     0x40012400
#define ADC1_CR1      (*(volatile uint32_t *)(ADC1_BASE + 0x04))
#define ADC1_CR2      (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR2    (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR1     (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
#define ADC1_SQR3     (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR       (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

#define TIM3_BASE     0x40000400
#define TIM3_CR1      (*(volatile uint32_t *)(TIM3_BASE + 0x00))
#define TIM3_CR2      (*(volatile uint32_t *)(TIM3_BASE + 0x04))
#define TIM3_EGR      (*(volatile uint32_t *)(TIM3_BASE + 0x14))
#define TIM3_PSC      (*(volatile uint32_t *)(TIM3_BASE + 0x28))
#define TIM3_ARR      (*(volatile uint32_t *)(TIM3_BASE + 0x2C))

#define DMA1_BASE     0x40020000
#define DMA1_ISR      (*(volatile uint32_t *)(DMA1_BASE + 0x00))
#define DMA1_IFCR     (*(volatile uint32_t *)(DMA1_BASE + 0x04))
#define DMA1_CCR1     (*(volatile uint32_t *)(DMA1_BASE + 0x08))
#define DMA1_CNDTR1   (*(volatile uint32_t *)(DMA1_BASE + 0x0C))
#define DMA1_CPAR1    (*(volatile uint32_t *)(DMA1_BASE + 0x10))
#define DMA1_CMAR1    (*(volatile uint32_t *)(DMA1_BASE + 0x14))

void hsadc_init(void) {
    RCC_AHBENR  |= (1 << 0);    // DMA1 clock
    RCC_APB2ENR |= (1 << 9);    // ADC1 clock
    RCC_APB2ENR |= (1 << 2);    // GPIOA clock
    RCC_APB1ENR |= (1 << 1);    // TIM3 clock

    // PA0 as analog input (MODE=00, CNF=00)
    GPIOA_CRL &= ~(0xF << 0);

    // ADC1: 1 conversion of channel 0, 13.5-cycle sample time,
    // triggered by TIM3 TRGO, result moved by DMA.
    ADC1_SQR1  = 0;                 // L = 0 → one conversion in the sequence
    ADC1_SQR3  = 0;                 // SQ1 = channel 0 (PA0)
    ADC1_SMPR2 = (0x2 << 0);        // channel 0 sample time = 13.5 cycles
    ADC1_CR1   = 0;                 // no scan
    ADC1_CR2   = (1 << 8)           // DMA enable
               | (0x4 << 17)        // EXTSEL = TIM3_TRGO
               | (1 << 20);         // EXTTRIG enable
    ADC1_CR2  |= (1 << 0);          // ADON (power up)
    for (volatile int i = 0; i < 10000; i++);   // tADC stabilisation

    // Calibrate the ADC (improves accuracy)
    ADC1_CR2 |= (1 << 3);           // RSTCAL
    while (ADC1_CR2 & (1 << 3));
    ADC1_CR2 |= (1 << 2);           // CAL
    while (ADC1_CR2 & (1 << 2));

    // TIM3: send its update event out as TRGO (which triggers the ADC)
    TIM3_CR2 = (0x2 << 4);          // MMS = 010 (update → TRGO)
    TIM3_PSC = 0;

    // DMA1 channel 1 (ADC1): peripheral → memory, 16-bit, memory-incrementing.
    DMA1_CPAR1 = (uint32_t)&ADC1_DR;
    DMA1_CCR1  = (0x1 << 8)         // PSIZE = 16-bit
               | (0x1 << 10)        // MSIZE = 16-bit
               | (1 << 7);          // MINC (advance memory each sample)
}

void hsadc_capture(uint16_t *buf, int n, uint32_t rate_hz) {
    if (rate_hz == 0) rate_hz = 1;
    TIM3_ARR = (TIM_CLK / rate_hz) - 1;         // set sample rate
    TIM3_EGR = (1 << 0);                          // UG: load ARR now

    DMA1_CCR1 &= ~(1 << 0);                       // disable while configuring
    DMA1_CMAR1  = (uint32_t)buf;
    DMA1_CNDTR1 = n;
    DMA1_IFCR   = (1 << 1);                        // clear channel-1 TC flag
    DMA1_CCR1  |= (1 << 0);                        // enable DMA

    TIM3_CR1 |= (1 << 0);                          // start timer → triggers ADC

    while (!(DMA1_ISR & (1 << 1)));                // wait for DMA complete (TCIF1)

    TIM3_CR1  &= ~(1 << 0);                        // stop timer
    DMA1_CCR1 &= ~(1 << 0);                        // disable DMA
}
