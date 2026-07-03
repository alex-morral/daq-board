#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
int i2c_read_reg16(uint8_t addr, uint8_t reg, uint16_t *value);
int i2c_write_reg16(uint8_t addr, uint8_t reg, uint16_t value);
int i2c_write_bytes(uint8_t addr, const uint8_t *data, int len);

#endif
