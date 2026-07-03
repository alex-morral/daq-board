# DAQ Board firmware — STM32F103C8T6 (bare-metal, no HAL)
#
# Requires arm-none-eabi-gcc and openocd on PATH.
# On Windows without `make`, use build.ps1 / flash.ps1 instead.

TARGET   = daq
BUILD    = build
SRC_DIR  = src

CC   = arm-none-eabi-gcc
SIZE = arm-none-eabi-size
OBJCOPY = arm-none-eabi-objcopy

SOURCES = $(wildcard $(SRC_DIR)/*.c)

CFLAGS  = -mcpu=cortex-m3 -mthumb -Os -Wall -Wextra
CFLAGS += -ffunction-sections -fdata-sections -nostdlib
CFLAGS += -I$(SRC_DIR)
LDFLAGS = -T stm32f103c8.ld -Wl,--gc-sections -nostdlib

ELF = $(BUILD)/$(TARGET).elf
BIN = $(BUILD)/$(TARGET).bin

.PHONY: all build flash clean

all: build

build: $(ELF)

$(ELF): $(SOURCES) stm32f103c8.ld | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES)
	$(OBJCOPY) -O binary $@ $(BIN)
	$(SIZE) $@

$(BUILD):
	mkdir -p $(BUILD)

flash: $(ELF)
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
	  -c "program $(ELF) verify reset exit"

clean:
	rm -rf $(BUILD)
