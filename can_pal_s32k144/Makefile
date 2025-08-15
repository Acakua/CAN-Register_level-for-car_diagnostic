# ==========================
# Project Configuration
# ==========================
TARGET      := main
CPU         := S32K144HFT0VLLT
BUILD_DIR   := build

CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size
MAKE     := "C:/Program Files (x86)/GnuWin32/bin/make"  # MAKE với path có dấu cách

# ==========================
# Include submodules
# ==========================
MODULE_OBJS :=

# Chỉ include để lấy biến MODULE_OBJS
include board/Makefile
include src/Makefile
include SDK/Makefile

# ==========================
# Linker Script
# ==========================
LDSCRIPT := Project_Settings/Linker_Files/S32K144_64_flash.ld

# ==========================
# Build flags
# ==========================
CFLAGS  := -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS  += -DCPU_$(CPU) -DS32K14x_SERIES
ASFLAGS := $(CFLAGS)
LDFLAGS := -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections

# ==========================
# Targets
# ==========================
.PHONY: all modules clean

all: modules $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

# Build các module trước
modules:
	$(MAKE) -C board
	$(MAKE) -C src
	$(MAKE) -C SDK

# Link ELF
$(BUILD_DIR)/$(TARGET).elf: $(MODULE_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# BIN từ ELF
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# HEX từ ELF
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# Dọn dẹp toàn bộ
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(MODULE_OBJS)

