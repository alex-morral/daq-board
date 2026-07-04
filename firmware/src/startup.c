#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata;
extern uint32_t _sbss, _ebss;

int main(void);

void Reset_Handler(void) {
    // Copy .data from flash to RAM
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    // Zero .bss
    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0;

    main();
    while (1);
}

void Default_Handler(void) {
    while (1);
}

void *memset(void *s, int c, unsigned int n) {
    uint8_t *p = s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

// Cortex-M3 core vector table (entries 0-15). External peripheral IRQs are
// omitted because this firmware enables no interrupts; all faults trap in
// Default_Handler instead of jumping to garbage.
__attribute__((section(".isr_vector")))
void (*const vectors[])(void) = {
    (void (*)(void))(&_estack),  //  0 Initial stack pointer
    Reset_Handler,               //  1 Reset
    Default_Handler,             //  2 NMI
    Default_Handler,             //  3 HardFault
    Default_Handler,             //  4 MemManage
    Default_Handler,             //  5 BusFault
    Default_Handler,             //  6 UsageFault
    0, 0, 0, 0,                  //  7-10 Reserved
    Default_Handler,             // 11 SVCall
    Default_Handler,             // 12 DebugMonitor
    0,                           // 13 Reserved
    Default_Handler,             // 14 PendSV
    Default_Handler,             // 15 SysTick
};
