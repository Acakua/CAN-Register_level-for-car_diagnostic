# ==========================
# Project Configuration
# ==========================
TARGET      := main
CPU         := S32K144HFT0VLLT
BUILD_DIR   := build

# ==========================
# Toolchain
# ==========================
CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size

# ==========================
# Flags
# ==========================
CFLAGS  := -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS  += -DCPU_$(CPU) -DS32K14x_SERIES
ASFLAGS := $(CFLAGS)


# ==========================
# Base Paths
# ==========================
CMSIS_DIR   := ./CMSIS
DRV_DIR     := ./drivers
SRC_DIR     := ./src
BOARD_DIR   := ./board
STARTUP_DIR := ./startup
SDK_DIR     := ./SDK
DEV_DIR     := $(SDK_DIR)/platform/devices/S32K144
DRV_SRC_DIR := $(SDK_DIR)/platform/drivers/src
PAL_DIR     := $(SDK_DIR)/platform/pal
RTOS_DIR    := $(SDK_DIR)/rtos
PS_DIR      := ./Project_Settings

# ==========================
# Include Paths
# ==========================
INCLUDE_DIRS := \
  $(CMSIS_DIR) \
  $(CMSIS_DIR)/devices \
  $(CMSIS_DIR)/devices/S32K144 \
  $(DRV_DIR) \
  $(SRC_DIR) \
  $(BOARD_DIR) \
  $(STARTUP_DIR) \
  $(SDK_DIR) \
  $(SDK_DIR)/platform \
  $(SDK_DIR)/platform/devices \
  $(DEV_DIR) \
  $(DEV_DIR)/startup \
  $(SDK_DIR)/platform/drivers \
  $(SDK_DIR)/platform/drivers/inc \
  $(DRV_SRC_DIR)/adc \
  $(DRV_SRC_DIR)/clock/S32K1xx \
  $(DRV_SRC_DIR)/pins \
  $(DRV_SRC_DIR)/interrupt \
  $(PAL_DIR)/inc \
  $(RTOS_DIR) \
  $(RTOS_DIR)/osif \
  ./S32K144/include \
  $(PS_DIR)/Startup_Code

INCLUDES := $(addprefix -I,$(INCLUDE_DIRS))

# ==========================
# Linker Script
# ==========================
LDSCRIPT := $(SDK_DIR)/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld

# ==========================
# Source Files (Auto Search)
# ==========================
SRC_DIRS := \
	$(SRC_DIR) \
	$(BOARD_DIR) \
	$(PS_DIR)/Startup_Code \
	$(DEV_DIR)/startup \
	$(SDK_DIR)/platform/devices \
	$(DRV_SRC_DIR)/clock/S32K1xx \
	$(DRV_SRC_DIR)/interrupt \
	$(DRV_SRC_DIR)/pins

# foreach: duyệt từng thư mục trong SRC_DIRS, 
# wildcard: tìm tất cả file có đuôi .c hoặc .S trong thư mục đó, 
# ":=" : gom thành danh sách lưu vào SRCS,
# Không cần quét lại file nếu không thêm hoặc xoá file
SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.[cS]))

# ==========================
# Objects & Outputs
# ==========================
OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c,$(SRCS)))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(filter %.S,$(SRCS)))

ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
HEX := $(BUILD_DIR)/$(TARGET).hex

# ==========================
# Rules
# ==========================
all: $(ELF) $(HEX) $(BIN)
	$(SIZE) $(ELF)

$(ELF): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -T $(LDSCRIPT) -Wl,--gc-sections -lnosys -o $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

flash: $(BIN)
	pyocd flash $(BIN)

.PHONY: all clean flash
