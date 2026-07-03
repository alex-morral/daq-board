#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_init(void);
uint8_t spi_transfer(uint8_t data);
void flash_cs_low(void);
void flash_cs_high(void);

#endif
