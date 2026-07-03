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

void uart_init(void) {
    // Enable clocks: GPIOA + USART1
    RCC_APB2ENR |= (1 << 2) | (1 << 14);   // IOPAEN + USART1EN

    // PA9 (TX): AF push-pull 50MHz = 0xB
    // PA10 (RX): input floating = 0x4
    GPIOA_CRH &= ~(0xFF << 4);
    GPIOA_CRH |=  (0xB << 4) | (0x4 << 8);

    // 9600 baud @ 8MHz HSI: BRR = 8000000 / 9600 = 833 = 0x341
    USART1_BRR = 0x341;
    // Enable USART, TX, RX
    USART1_CR1 = (1 << 13) | (1 << 3) | (1 << 2);
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
