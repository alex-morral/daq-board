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

// Vector table
__attribute__((section(".isr_vector")))
void (*const vectors[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    Default_Handler,  // NMI
    Default_Handler,  // HardFault
    Default_Handler,  // MemManage
    Default_Handler,  // BusFault
    Default_Handler,  // UsageFault
};
