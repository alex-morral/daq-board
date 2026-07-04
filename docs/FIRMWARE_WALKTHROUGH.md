# Firmware walkthrough — how it all works

A guided read of the bare-metal firmware, written so you can understand every
line and then modify it yourself. No HAL, no libraries — just the STM32F103
reference manual and C.

> Read this with the source open in split view. Order: `startup.c` →
> `stm32f103c8.ld` → `uart.c` → `i2c.c` → `spi.c` → `main.c`.

---

## 0. The one idea behind everything: memory-mapped registers

On a microcontroller, hardware is controlled by **writing to fixed memory
addresses**. Each peripheral (GPIO, I²C, UART…) has a block of 32-bit
"registers" at a known address. Writing a bit turns something on; reading a bit
tells you a status.

Example from the code:

```c
#define GPIOB_BASE  0x40010C00
#define GPIOB_ODR  (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))
GPIOB_ODR ^= (1 << 0);   // toggle bit 0 of port B output → toggles PB0 (LED2)
```

- `0x40010C00` is where GPIOB lives (from the reference manual memory map).
- `+ 0x0C` is the offset of the **ODR** (Output Data Register).
- `*(volatile uint32_t *)ADDR` means "treat this address as a 32-bit register".
- `volatile` tells the compiler *never* to optimise the access away — the value
  can change because of hardware, not just code.

Every driver in this project is just: enable the peripheral's clock, configure
its pins, write config registers, then read/write data registers. That's it.

---

## 1. `startup.c` — what runs before `main()`

When the STM32 powers up it does **not** jump straight to `main()`. It reads the
**vector table** — an array of addresses at the very start of flash:

```c
__attribute__((section(".isr_vector")))
void (*const vectors[])(void) = {
    (void (*)(void))(&_estack),  // [0] initial stack pointer
    Reset_Handler,               // [1] where the CPU starts executing
    ...
};
```

- Entry 0 = initial **stack pointer** value.
- Entry 1 = **Reset_Handler**, the first code that runs.
- The rest are fault/interrupt handlers (we point them at `Default_Handler`
  because this firmware uses no interrupts).

`Reset_Handler` does the C runtime setup the compiler assumes exists:

```c
void Reset_Handler(void) {
    // copy initialised globals from flash → RAM (.data)
    // zero uninitialised globals (.bss)
    main();
    while (1);   // never return
}
```

Globals with a value live in flash but must be *copied* to RAM before `main`;
zero-initialised globals must be *cleared*. On a PC the OS does this — here we do
it by hand. `_sdata`, `_edata`, `_sbss`… come from the linker script (next).

We also define our own `memset` because with `-nostdlib` there's no C library.

---

## 2. `stm32f103c8.ld` — the memory map

The linker script tells the compiler **where things go** in the chip:

```
MEMORY {
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K   /* code + constants */
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K   /* variables + stack */
}
```

- **Flash** (0x0800_0000): non-volatile, holds the program. 64 KB on the C8 part.
- **RAM** (0x2000_0000): volatile, holds variables and the stack. 20 KB.

The `SECTIONS` block places code (`.text`), constants (`.rodata`), initialised
data (`.data`) and zero data (`.bss`), and exports the symbols
(`_sdata`, `_sbss`, `_estack`…) that `startup.c` uses. `_estack` is set to the
top of RAM because the Cortex-M stack grows downward.

---

## 3. `uart.c` — talking to the PC

USART1 sends bytes to the CH340 chip, which turns them into USB serial (your COM
port). Setup:

1. Enable clocks for GPIOA and USART1 (`RCC_APB2ENR`).
2. Configure PA9 as TX (alternate-function push-pull), PA10 as RX (input).
3. Set the **baud rate** via `BRR = clock / baud`. At 8 MHz for 115200 →
   `8_000_000 / 115200 ≈ 69 = 0x45`.
4. Enable the USART.

Sending a byte is a two-step handshake:

```c
void uart_send_char(char c){
    while (!(USART1_SR & (1 << 7)));  // wait until TXE (transmit buffer empty)
    USART1_DR = c;                     // write the byte
}
```

This is **blocking** — the CPU spins until the byte can be queued. Fine here;
the "roadmap" replaces it with interrupt/DMA-driven TX for higher throughput.

---

## 4. `i2c.c` — the sensor/DAC bus

I²C is a 2-wire bus (SCL clock, SDA data) shared by the TMP117, ADS1115 and
MCP4725. Each device has a 7-bit address (0x49, 0x48, 0x60). A transaction is a
sequence of hardware events you drive through registers and wait on via status
flags:

```
START → send address+W → send register → repeated START → address+R → read bytes → STOP
```

`wait_flag()` polls a status bit with a timeout so a missing device can't hang
the firmware forever. **Robustness added in the audit:**

- `i2c_abort()` fires a STOP on every error path, so a failed transfer releases
  the bus instead of leaving SCL/SDA latched.
- `i2c_ensure_ready()` runs before each transaction: if the peripheral is stuck
  BUSY, it software-resets and re-initialises the I²C block (self-heal).

Without these, one NAK from a device could wedge the bus and every later read
would fail until a full reset.

---

## 5. `spi.c` — the flash memory

SPI is faster and simpler than I²C: separate lines for clock (SCK), data out
(MOSI), data in (MISO), plus a chip-select (CS) we drive by hand on PA4. A
transfer is full-duplex — you send and receive a byte at the same time:

```c
uint8_t spi_transfer(uint8_t data){
    while (!(SPI1_SR & (1<<1)));  // TXE: ok to write
    SPI1_DR = data;
    while (!(SPI1_SR & (1<<0)));  // RXNE: a byte came back
    return SPI1_DR;
}
```

The W25Q32 flash speaks a command protocol on top of this: `0x9F` reads the
JEDEC ID, `0x06` write-enable, `0x20` erase a 4 KB sector, `0x02` program a page,
`0x03` read. The bring-up test does erase → write "DAQ Board OK!" → read-back.

---

## 6. `main.c` — the application

Now the pieces combine. `main()`:

1. Inits UART, I²C, SPI and the status LEDs.
2. Prints a banner, sets the DAC to mid-scale, checks the flash ID.
3. Loops forever:
   - read TMP117 temperature (I²C),
   - read all 4 ADS1115 channels (`read_adc_channel`),
   - print one line `T:..|A0:..|A1:..|A2:..|A3:..|D:..|P:..`,
   - update the status LEDs,
   - short delay.

Two functions worth understanding:

**`read_adc_channel()`** — writes the ADS1115 config (which channel, gain,
single-shot), then **polls the OS bit** until the conversion is done before
reading the result. (Earlier it used a blind delay, which could read a stale
sample — fixed in the audit.) On any I²C failure it returns `ADC_ERR`.

**`send_voltage()`** — converts the raw 16-bit code to volts. The gain (PGA)
sets how many microvolts each bit is worth (`lsb_nv[]`). The multiply is done in
`int64_t` because `32767 × 187500` overflows a 32-bit int — a real bug caught in
the audit. If the reading was `ADC_ERR`, it prints `err` so the dashboard can
show a fault instead of a fake number.

### The command channel

The PC can talk back. `check_uart_rx()` reads incoming bytes into a buffer; on a
newline, `process_command()` parses:

- `DAC:<0-4095>` → set the DAC output voltage,
- `PGA:<0-5>` → change the ADC input range.

`check_uart_rx()` is called inside `delay()` too, so commands are handled even
while the loop is waiting.

---

## How to change something (try it)

- **Blink faster**: change the `delay(...)` at the bottom of the main loop.
- **Different LED**: `main.c` drives PB0/PB1/PC13 — swap the pin bits.
- **New command**: add another `if` in `process_command()` (e.g. `LED:1`).
- **Log to flash**: use the `flash_write_page`/`flash_read` helpers to store the
  ADC samples — that's roadmap item #2.

Build and flash after any change:

```
./build.ps1     # or: make
./flash.ps1     # or: make flash   (ST-Link on J3)
```
