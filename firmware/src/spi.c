#include "spi.h"

// SPI1: PA5=SCK, PA6=MISO, PA7=MOSI, PA4=CS (manual)
#define RCC_BASE        0x40021000
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define GPIOA_BASE      0x40010800
#define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))

#define SPI1_BASE       0x40013000
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

void spi_init(void) {
    // Enable clocks: GPIOA + SPI1
    RCC_APB2ENR |= (1 << 2) | (1 << 12);   // IOPAEN + SPI1EN

    // PA4 (CS): push-pull output 50MHz = 0x3
    // PA5 (SCK): AF push-pull 50MHz = 0xB
    // PA6 (MISO): input floating = 0x4
    // PA7 (MOSI): AF push-pull 50MHz = 0xB
    GPIOA_CRL &= ~(0xFFFF << 16);
    GPIOA_CRL |= ((uint32_t)0x3 << 16) | ((uint32_t)0xB << 20) | ((uint32_t)0x4 << 24) | ((uint32_t)0xB << 28);

    flash_cs_high();

    // SPI1 config: master, CPOL=0, CPHA=0, baud=fPCLK/8 (1MHz @ 8MHz)
    // BR[2:0]=010, MSTR=1, SPE=1, SSM=1, SSI=1
    SPI1_CR1 = (1 << 2) | (0x2 << 3) | (1 << 8) | (1 << 9) | (1 << 6);
}

uint8_t spi_transfer(uint8_t data) {
    while (!(SPI1_SR & (1 << 1)));      // Wait TXE
    SPI1_DR = data;
    while (!(SPI1_SR & (1 << 0)));      // Wait RXNE
    return SPI1_DR;
}

void flash_cs_low(void) {
    GPIOA_ODR &= ~(1 << 4);
}

void flash_cs_high(void) {
    GPIOA_ODR |= (1 << 4);
}
