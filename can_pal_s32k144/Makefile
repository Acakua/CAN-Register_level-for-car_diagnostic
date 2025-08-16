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
# Linker Script & Startup
# ==========================
LDSCRIPT     := Project_Settings/Linker_Files/S32K144_64_flash.ld
STARTUP_SRC  := Project_Settings/Startup_Code/startup_S32K144.S
STARTUP_OBJ  := $(BUILD_DIR)/startup_S32K144.o

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
.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

# --- Build dependency: các module + startup phải được build trước khi link ---
$(BUILD_DIR)/$(TARGET).elf: board/.built src/.built SDK/.built $(MODULE_OBJS) $(STARTUP_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(MODULE_OBJS) $(STARTUP_OBJ) $(LDFLAGS)

# Rule build file Assembly startup
$(STARTUP_OBJ): $(STARTUP_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# Đặt flag .built cho từng module
board/.built:
	$(MAKE) -C board
	@echo "done" > $@

src/.built:
	$(MAKE) -C src
	@echo "done" > $@

SDK/.built:
	$(MAKE) -C SDK
	@echo "done" > $@

# BIN từ ELF
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# HEX từ ELF
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# Dọn dẹp toàn bộ
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(MODULE_OBJS) $(STARTUP_OBJ)
	rm -f board/.built src/.built SDK/.built
