#include <stdint.h>
#include "i2c.h"
#include "uart.h"
#include "spi.h"
#include "timer.h"

#define GPIOB_BASE      0x40010C00
#define GPIOB_CRL       (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))

#define GPIOC_BASE      0x40011000
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

#define RCC_BASE2       0x40021000
#define RCC_APB2ENR2    (*(volatile uint32_t *)(RCC_BASE2 + 0x18))

// Status LEDs: D1=PC13 (source mode, dim), D2=PB0 (heartbeat), D3=PB1 (comms)
#define LED_DAC_ON()    (GPIOC_ODR |=  (1 << 13))   // D1 on
#define LED_DAC_OFF()   (GPIOC_ODR &= ~(1 << 13))   // D1 off
#define LED_HB_TOGGLE() (GPIOB_ODR ^=  (1 << 0))    // D2 toggle
#define LED_COMM_ON()   (GPIOB_ODR |=  (1 << 1))    // D3 on
#define LED_COMM_OFF()  (GPIOB_ODR &= ~(1 << 1))    // D3 off

#define USART1_BASE     0x40013800
#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04))

#define TMP117_ADDR     0x49
#define TMP117_TEMP_REG 0x00

#define ADS1115_ADDR    0x48
#define ADS1115_CONV    0x00
#define ADS1115_CONFIG  0x01

#define MCP4725_ADDR    0x60

static uint16_t dac_value = 2048;
static uint8_t adc_pga = 1; // 0=6.144V, 1=4.096V, 2=2.048V, 3=1.024V, 4=0.512V, 5=0.256V

// PGA LSB sizes in nanovolts, indexed by pga setting
// (187.5, 125, 62.5, 31.25, 15.625, 7.8125 uV/bit)
static const int32_t lsb_nv[] = { 187500, 125000, 62500, 31250, 15625, 7813 };

// Sentinel returned when an ADC / I2C read fails, so the host can tell a real
// measurement from a bus error instead of showing a plausible-looking voltage.
#define ADC_ERR  0x7FFFFFFF

static int mcp4725_write(uint16_t value_12bit) {
    uint8_t data[2];
    data[0] = (value_12bit >> 8) & 0x0F;
    data[1] = value_12bit & 0xFF;
    return i2c_write_bytes(MCP4725_ADDR, data, 2);
}

// ---------------------------------------------------------------------------
// Function generator (DAC + timer, Direct Digital Synthesis)
//
// A hardware timer ticks at a fixed sample rate GEN_FS. On each tick a 32-bit
// phase accumulator advances by a "tuning word"; its top 6 bits index a
// 64-point wavetable, and that value is written to the DAC. Output frequency =
// tuning_word / 2^32 * GEN_FS, so we get fine frequency control from one table.
// This is exactly how a DDS signal generator works.
// ---------------------------------------------------------------------------
#define GEN_FS   2000u    // DAC update rate (Hz); bounded by I2C write time

// One cycle of a sine, 64 points, scaled to the 12-bit DAC range (0..4095).
static const uint16_t sine64[64] = {
    2048,2249,2447,2642,2831,3013,3185,3347,3495,3630,3750,3853,3939,4007,4056,4085,
    4095,4085,4056,4007,3939,3853,3750,3630,3495,3347,3185,3013,2831,2642,2447,2249,
    2048,1847,1649,1454,1265,1083, 911, 749, 601, 466, 346, 243, 157,  89,  40,  11,
       1,  11,  40,  89, 157, 243, 346, 466, 601, 749, 911,1083,1265,1454,1649,1847
};

static volatile uint8_t  gen_mode  = 0;   // 0=off 1=sine 2=square 3=triangle 4=saw
static volatile uint16_t gen_freq  = 0;   // requested output frequency (Hz)
static uint32_t gen_phase = 0;            // DDS phase accumulator
static uint32_t gen_tw    = 0;            // DDS tuning word

static void gen_set(uint8_t mode, uint16_t freq) {
    if (freq > 500) freq = 500;           // keep waveforms well-sampled
    gen_mode = mode;
    gen_freq = freq;
    gen_phase = 0;
    // tuning_word = freq * 2^32 / Fs  (64-bit avoids overflow)
    gen_tw = (uint32_t)(((uint64_t)freq << 32) / GEN_FS);
    if (mode == 0) mcp4725_write(dac_value);   // restore manual DAC level
}

// Produce one output sample from the current phase, per waveform.
static uint16_t gen_sample(void) {
    uint32_t p = gen_phase;
    switch (gen_mode) {
        case 1: return sine64[p >> 26];                     // sine (table)
        case 2: return (p & 0x80000000u) ? 4095 : 0;        // square
        case 3: {                                            // triangle
            uint32_t s = p >> 20;                            // 0..4095
            return s < 2048 ? (s * 2) : ((4095 - s) * 2);
        }
        case 4: return p >> 20;                              // sawtooth
        default: return dac_value;
    }
}

// A completed command line, filled by the USART1 interrupt and consumed by the
// main loop. The ISR only buffers text (never touches I2C), so the main loop is
// free to run the actual command — which does I2C — at a safe point.
static volatile char    cmd_buf[24];
static volatile uint8_t cmd_ready = 0;
static volatile int     comm_activity = 0;

// USART1 receive interrupt: captures each incoming byte the moment it arrives,
// so no command byte is ever lost, even while the main loop is busy driving the
// DAC. Assembles bytes into a line and flags it complete on newline.
void USART1_IRQHandler(void) {
    static char line[24];
    static uint8_t idx = 0;
    while (USART1_SR & (1 << 5)) {          // RXNE: a byte is waiting
        char c = USART1_DR;                  // reading DR clears the flag
        if (c == '\n' || c == '\r') {
            if (idx > 0) {
                for (uint8_t i = 0; i < idx; i++) cmd_buf[i] = line[i];
                cmd_buf[idx] = 0;            // null-terminate
                cmd_ready = 1;
                idx = 0;
            }
        } else if (idx < sizeof(line) - 1) {
            line[idx++] = c;
        }
    }
}

// Parse and act on the last completed command (runs in main context, so I2C is
// safe here). cmd_buf is a null-terminated string like "DAC:2048" or "GEN:1:50".
static void process_command(void) {
    comm_activity = 8;  // light D3 for next 8 loop iterations
    const volatile char *b = cmd_buf;

    if (b[0]=='D' && b[1]=='A' && b[2]=='C' && b[3]==':') {
        uint16_t val = 0;
        for (int i = 4; b[i]; i++)
            if (b[i] >= '0' && b[i] <= '9') val = val * 10 + (b[i] - '0');
        if (val > 4095) val = 4095;
        dac_value = val;
        mcp4725_write(dac_value);
    }
    else if (b[0]=='P' && b[1]=='G' && b[2]=='A' && b[3]==':') {
        uint8_t val = b[4] - '0';
        if (val <= 5) adc_pga = val;
    }
    // GEN:<mode>:<freq>  e.g. "GEN:1:50" = sine 50 Hz, "GEN:0" = off
    else if (b[0]=='G' && b[1]=='E' && b[2]=='N' && b[3]==':') {
        uint8_t mode = b[4] - '0';
        uint16_t freq = 0;
        int i = 5;
        if (b[i] == ':') i++;                            // skip separator
        for (; b[i]; i++)
            if (b[i] >= '0' && b[i] <= '9') freq = freq * 10 + (b[i] - '0');
        if (mode <= 4) gen_set(mode, freq);
    }
}

// Run any pending command. Called from safe points in the main loop.
static void poll_command(void) {
    if (cmd_ready) {
        process_command();
        cmd_ready = 0;
    }
}

void delay(volatile uint32_t count) {
    while (count--);
}

// Read one single-ended ADC channel. Returns the signed 16-bit code, or
// ADC_ERR if any I2C transaction fails. Waits for the conversion by polling
// the ADS1115 OS bit (config reg bit 15 = 1 when the conversion is done),
// instead of a blind delay that could return a stale sample.
static int32_t read_adc_channel(uint8_t channel) {
    // MUX: 100=AIN0, 101=AIN1, 110=AIN2, 111=AIN3 (single-ended vs GND)
    uint16_t mux = (0x04 + channel) << 12;
    uint16_t pga = (uint16_t)adc_pga << 9;
    // OS=1 (start), MODE=1 (single-shot), DR=100 (128SPS), COMP=11
    uint16_t cfg = (1 << 15) | mux | pga | (1 << 8) | (0x4 << 5) | 0x03;

    if (i2c_write_reg16(ADS1115_ADDR, ADS1115_CONFIG, cfg) != 0)
        return ADC_ERR;

    // Poll OS bit until conversion complete (bounded, ~few ms at 128 SPS)
    uint16_t status = 0;
    for (int tries = 0; tries < 50; tries++) {
        if (i2c_read_reg16(ADS1115_ADDR, ADS1115_CONFIG, &status) != 0)
            return ADC_ERR;
        if (status & 0x8000) break;   // OS=1 -> ready
        delay(2000);
    }

    uint16_t raw = 0;
    if (i2c_read_reg16(ADS1115_ADDR, ADS1115_CONV, &raw) != 0)
        return ADC_ERR;

    return (int16_t)raw;   // sign-extend 16-bit code
}

// Print a channel value as signed volts (e.g. "1.650", "-0.012") or "err".
static void send_voltage(int32_t raw) {
    if (raw == ADC_ERR) { uart_send_string("err"); return; }

    // 64-bit multiply avoids int32 overflow (32767 * 187500 > 2^31)
    int64_t nv = (int64_t)raw * lsb_nv[adc_pga];   // nanovolts
    int32_t mv = (int32_t)(nv / 1000000);          // millivolts, range -6144..6144

    if (mv < 0) { uart_send_char('-'); mv = -mv; }
    int volts = mv / 1000;
    int frac  = mv % 1000;
    uart_send_int(volts);
    uart_send_char('.');
    if (frac < 100) uart_send_char('0');
    if (frac < 10)  uart_send_char('0');
    uart_send_int(frac);
}

int main(void) {
    uart_init();
    i2c_init();
    spi_init();
    timer_init(GEN_FS);                    // TIM2 tick source for the generator

    // Status LEDs as push-pull outputs (2MHz)
    RCC_APB2ENR2 |= (1 << 4);              // enable GPIOC clock (for PC13)
    GPIOB_CRL &= ~(0xFF << 0);             // PB0 (D2) + PB1 (D3)
    GPIOB_CRL |=  (0x2 << 0) | (0x2 << 4);
    GPIOC_CRH &= ~(0xF << 20);             // PC13 (D1)
    GPIOC_CRH |=  (0x2 << 20);
    LED_COMM_OFF();

    delay(3000000);

    uart_send_string("\r\nDAQ Board v2.0\r\n");
    uart_send_string("================\r\n\r\n");

    // MCP4725 initial
    mcp4725_write(dac_value);
    uart_send_string("[OK] MCP4725 ready\r\n");

    // W25Q32 JEDEC ID
    flash_cs_low();
    spi_transfer(0x9F);
    uint8_t mfr = spi_transfer(0x00);
    uint8_t mem_type = spi_transfer(0x00);
    uint8_t capacity = spi_transfer(0x00);
    flash_cs_high();

    uint8_t flash_ok = (mfr == 0xEF && mem_type == 0x40 && capacity == 0x16);
    uart_send_string(flash_ok ? "[OK] W25Q32 ready\r\n" : "[!!] W25Q32 error\r\n");

    uart_send_string("\r\n");

    uint32_t gen_status = 0;

    while (1) {
        poll_command();

        // --- Function-generator mode ---------------------------------------
        // When active we dedicate the loop to steady waveform output (no sensor
        // I2C, which shares the same bus). Each timer tick advances the DDS
        // phase and drives the DAC; a status line is sent ~4x/second.
        if (gen_mode) {
            timer_wait_tick();
            gen_phase += gen_tw;
            mcp4725_write(gen_sample());
            if (++gen_status >= GEN_FS / 4) {
                gen_status = 0;
                uart_send_string("G:");
                uart_send_int(gen_mode);
                uart_send_char(':');
                uart_send_int(gen_freq);
                uart_send_string("\r\n");
                LED_HB_TOGGLE();
                LED_DAC_ON();
            }
            continue;
        }

        // TMP117
        uint16_t raw = 0;
        int temp_ok = (i2c_read_reg16(TMP117_ADDR, TMP117_TEMP_REG, &raw) == 0);
        int temp_x100 = temp_ok ? (((int16_t)raw) * 100) / 128 : 0;

        // Read all 4 ADC channels
        int32_t ch[4];
        for (int i = 0; i < 4; i++) {
            ch[i] = read_adc_channel(i);
        }

        // Output format: T:29.60|A0:0.580|A1:0.123|A2:0.456|A3:0.789|D:2048|P:1
        // A failed read is sent as "err" so the host can distinguish it.
        uart_send_string("T:");
        if (temp_ok) {
            int integer = temp_x100 / 100;
            int decimal = temp_x100 % 100;
            if (decimal < 0) decimal = -decimal;
            uart_send_int(integer);
            uart_send_char('.');
            if (decimal < 10) uart_send_char('0');
            uart_send_int(decimal);
        } else {
            uart_send_string("err");
        }

        for (int i = 0; i < 4; i++) {
            uart_send_string("|A");
            uart_send_char('0' + i);
            uart_send_char(':');
            send_voltage(ch[i]);
        }

        uart_send_string("|D:");
        uart_send_int(dac_value);
        uart_send_string("|P:");
        uart_send_char('0' + adc_pga);

        uart_send_string("\r\n");

        // Status LEDs
        LED_HB_TOGGLE();                        // D2: heartbeat
        if (dac_value > 0) LED_DAC_ON();        // D1: DAC output active
        else LED_DAC_OFF();
        if (comm_activity > 0) {                // D3: comms activity
            comm_activity--;
            LED_COMM_ON();
        } else {
            LED_COMM_OFF();
        }

        delay(60000);
    }
}
