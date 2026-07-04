#include "i2c.h"

// STM32F103 I2C1 registers (PB6=SCL, PB7=SDA)
#define RCC_BASE        0x40021000
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1C))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define GPIOB_BASE      0x40010C00
#define GPIOB_CRL       (*(volatile uint32_t *)(GPIOB_BASE + 0x00))

#define I2C1_BASE       0x40005400
#define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04))
#define I2C1_OAR1       (*(volatile uint32_t *)(I2C1_BASE + 0x08))
#define I2C1_DR         (*(volatile uint32_t *)(I2C1_BASE + 0x10))
#define I2C1_SR1        (*(volatile uint32_t *)(I2C1_BASE + 0x14))
#define I2C1_SR2        (*(volatile uint32_t *)(I2C1_BASE + 0x18))
#define I2C1_CCR        (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
#define I2C1_TRISE      (*(volatile uint32_t *)(I2C1_BASE + 0x20))

#define TIMEOUT 100000

static int wait_flag(volatile uint32_t *reg, uint32_t flag) {
    uint32_t t = TIMEOUT;
    while (!(*reg & flag) && --t);
    return t ? 0 : -1;
}

// Release the bus after an error: generate STOP so peripherals/lines don't
// stay latched. Returns -1 for convenient `return i2c_abort();`.
static int i2c_abort(void) {
    I2C1_CR1 |= (1 << 9);   // STOP
    return -1;
}

// If the peripheral is stuck BUSY (e.g. a previous aborted transfer), a
// software reset clears it and re-initialises. Called at the start of each
// transaction as a cheap self-heal.
static void i2c_ensure_ready(void) {
    uint32_t t = TIMEOUT;
    while ((I2C1_SR2 & (1 << 1)) && --t);   // wait while BUSY
    if (t == 0) {
        // Bus still busy: reset the I2C peripheral and reconfigure.
        I2C1_CR1 |= (1 << 15);              // SWRST
        I2C1_CR1 &= ~(1 << 15);
        I2C1_CR2 = 8;
        I2C1_CCR = 40;
        I2C1_TRISE = 9;
        I2C1_CR1 |= (1 << 0);              // PE
    }
}

void i2c_init(void) {
    // Enable clocks: GPIOB + I2C1
    RCC_APB2ENR |= (1 << 3);           // IOPBEN
    RCC_APB1ENR |= (1 << 21);          // I2C1EN

    // PB6 (SCL) and PB7 (SDA): alternate function open-drain
    // CRL[27:24] = PB6 = 0xF (50MHz AF open-drain)
    // CRL[31:28] = PB7 = 0xF (50MHz AF open-drain)
    GPIOB_CRL &= ~(0xFF << 24);
    GPIOB_CRL |=  (0xFF << 24);

    // Reset I2C
    I2C1_CR1 |= (1 << 15);     // SWRST
    I2C1_CR1 &= ~(1 << 15);

    // Configure I2C: 8MHz APB1, 100kHz standard mode
    I2C1_CR2 = 8;               // FREQ = 8MHz (HSI)
    I2C1_CCR = 40;              // CCR = 8MHz / (2 * 100kHz)
    I2C1_TRISE = 9;             // TRISE = 8 + 1
    I2C1_CR1 |= (1 << 0);      // PE (peripheral enable)
}

int i2c_read_reg16(uint8_t addr, uint8_t reg, uint16_t *value) {
    i2c_ensure_ready();

    // Send START + address + register
    I2C1_CR1 |= (1 << 8);                                    // START
    if (wait_flag(&I2C1_SR1, 1 << 0)) return i2c_abort();    // SB

    I2C1_DR = (addr << 1) | 0;                                // Address + Write
    if (wait_flag(&I2C1_SR1, 1 << 1)) return i2c_abort();    // ADDR
    (void)I2C1_SR2;                                            // Clear ADDR

    I2C1_DR = reg;                                             // Register address
    if (wait_flag(&I2C1_SR1, 1 << 7)) return i2c_abort();    // TxE

    // Repeated START for read
    I2C1_CR1 |= (1 << 8);                                    // START
    if (wait_flag(&I2C1_SR1, 1 << 0)) return i2c_abort();    // SB

    I2C1_CR1 |= (1 << 10);                                    // ACK
    I2C1_DR = (addr << 1) | 1;                                // Address + Read
    if (wait_flag(&I2C1_SR1, 1 << 1)) return i2c_abort();    // ADDR
    (void)I2C1_SR2;                                            // Clear ADDR

    // Read MSB (ACK)
    if (wait_flag(&I2C1_SR1, 1 << 6)) return i2c_abort();    // RxNE
    uint8_t msb = I2C1_DR;

    // Read LSB (NACK + STOP)
    I2C1_CR1 &= ~(1 << 10);                                   // NACK
    I2C1_CR1 |= (1 << 9);                                     // STOP
    if (wait_flag(&I2C1_SR1, 1 << 6)) return -1;             // RxNE (STOP already set)
    uint8_t lsb = I2C1_DR;

    *value = (msb << 8) | lsb;
    return 0;
}

int i2c_write_reg16(uint8_t addr, uint8_t reg, uint16_t value) {
    i2c_ensure_ready();

    I2C1_CR1 |= (1 << 8);                                    // START
    if (wait_flag(&I2C1_SR1, 1 << 0)) return i2c_abort();    // SB

    I2C1_DR = (addr << 1) | 0;                                // Address + Write
    if (wait_flag(&I2C1_SR1, 1 << 1)) return i2c_abort();    // ADDR
    (void)I2C1_SR2;

    I2C1_DR = reg;
    if (wait_flag(&I2C1_SR1, 1 << 7)) return i2c_abort();    // TxE

    I2C1_DR = (value >> 8);                                   // MSB
    if (wait_flag(&I2C1_SR1, 1 << 7)) return i2c_abort();

    I2C1_DR = (value & 0xFF);                                 // LSB
    if (wait_flag(&I2C1_SR1, 1 << 7)) return i2c_abort();

    I2C1_CR1 |= (1 << 9);                                     // STOP
    return 0;
}

int i2c_write_bytes(uint8_t addr, const uint8_t *data, int len) {
    i2c_ensure_ready();

    I2C1_CR1 |= (1 << 8);                                    // START
    if (wait_flag(&I2C1_SR1, 1 << 0)) return i2c_abort();    // SB

    I2C1_DR = (addr << 1) | 0;                                // Address + Write
    if (wait_flag(&I2C1_SR1, 1 << 1)) return i2c_abort();    // ADDR
    (void)I2C1_SR2;

    for (int i = 0; i < len; i++) {
        I2C1_DR = data[i];
        if (wait_flag(&I2C1_SR1, 1 << 7)) return i2c_abort();
    }

    I2C1_CR1 |= (1 << 9);                                     // STOP
    return 0;
}
