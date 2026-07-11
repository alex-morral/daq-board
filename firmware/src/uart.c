#include "uart.h"
#include <stdint.h>

// USART1: PA9=TX, PA10=RX → CH340 → USB COM3
#define RCC_BASE        0x40021000
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define GPIOA_BASE      0x40010800
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04))

#define USART1_BASE     0x40013800
#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0C))

// NVIC (interrupt controller) set-enable register 1 (IRQ 32..63)
#define NVIC_ISER1      (*(volatile uint32_t *)0xE000E104)

void uart_init(void) {
    // Enable clocks: GPIOA + USART1
    RCC_APB2ENR |= (1 << 2) | (1 << 14);   // IOPAEN + USART1EN

    // PA9 (TX): AF push-pull 50MHz = 0xB
    // PA10 (RX): input floating = 0x4
    GPIOA_CRH &= ~(0xFF << 4);
    GPIOA_CRH |=  (0xB << 4) | (0x4 << 8);

    // 115200 baud @ 8MHz HSI: BRR = 8000000 / 115200 = 69 = 0x45 (0.6% error, OK)
    USART1_BRR = 0x45;
    // Enable USART, TX, RX, and the RX-not-empty interrupt (RXNEIE)
    USART1_CR1 = (1 << 13) | (1 << 5) | (1 << 3) | (1 << 2);

    // Enable USART1 interrupt in the NVIC (IRQ 37 → ISER1 bit 5). This lets an
    // incoming byte be captured the instant it arrives, so commands are never
    // lost while the main loop is busy (e.g. driving the DAC in generator mode).
    NVIC_ISER1 |= (1 << 5);
}

void uart_send_char(char c) {
    while (!(USART1_SR & (1 << 7)));    // Wait TXE
    USART1_DR = c;
}

void uart_send_string(const char *s) {
    while (*s) uart_send_char(*s++);
}

void uart_send_int(int value) {
    if (value < 0) {
        uart_send_char('-');
        value = -value;
    }
    char buf[12];
    int i = 0;
    if (value == 0) {
        uart_send_char('0');
        return;
    }
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i > 0) uart_send_char(buf[--i]);
}
