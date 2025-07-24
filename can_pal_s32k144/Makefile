# Makefile cho S32K144 với GCC toolchain

# Tên project
TARGET = main
CPU = S32K144HFT0VLLT

# Toolchain
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Flags
CFLAGS = -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS += -DCPU_$(CPU)
#CFLAGS += -DS32K144_SERIES
CFLAGS += -DS32K14x_SERIES


ASFLAGS = $(CFLAGS)  # Dùng chung flag cho file .S

# Thư mục output
BUILD_DIR = build

# Đường dẫn include
INCLUDES = \
  -I./CMSIS \
  -I./CMSIS/devices \
  -I./CMSIS/devices/S32K144 \
  -I./drivers \
  -I./src \
  -I./board \
  -I./startup \
  -I./SDK \
  -I./SDK/platform \
  -I./SDK/platform/devices \
  -I./SDK/platform/devices/S32K144 \
  -I./SDK/platform/devices/S32K144/startup \
  -I./SDK/platform/drivers \
  -I./SDK/platform/drivers/inc \
  -I./SDK/platform/drivers/src/adc \
  -I./SDK/platform/drivers/src/clock/S32K1xx \
  -I./SDK/platform/drivers/src/pins \
  -I./SDK/platform/drivers/src/interrupt \
  -I./SDK/platform/pal/inc \
  -I./SDK/rtos \
  -I./SDK/rtos/osif \
  -I./S32K144/include \
  -I./Project_Settings/Startup_Code

# File linker script
LDSCRIPT = ./SDK/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld

# File nguồn
SRCS = \
  src/main.c \
  src/FlexCan.c \
  board/clock_config.c \
  board/pin_mux.c \
  Project_Settings/Startup_Code/startup_s32K144.S \
  SDK/platform/devices/S32K144/startup/system_S32K144.c \
  SDK/platform/devices/startup.c \
  SDK/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c \
  SDK/platform/drivers/src/interrupt/interrupt_manager.c \
  SDK/platform/drivers/src/pins/pins_driver.c \
  SDK/platform/drivers/src/pins/pins_port_hw_access.c

# Tạo file .o từ .c/.S
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c,$(SRCS)))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(filter %.S,$(SRCS)))

# File đầu ra
ELF = $(BUILD_DIR)/$(TARGET).elf
BIN = $(BUILD_DIR)/$(TARGET).bin
HEX = $(BUILD_DIR)/$(TARGET).hex

# Luật chính
all: $(ELF) $(HEX) $(BIN)
	$(SIZE) $(ELF)

# Build ELF
$(ELF): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -T $(LDSCRIPT) -Wl,--gc-sections -lnosys -o $@

# Tạo HEX và BIN
$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

# Build file .c
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build file .S
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(INCLUDES) -c $< -o $@

# Dọn dẹp
clean:
	rm -rf $(BUILD_DIR)

# Nạp chương trình
flash: $(BIN)
	pyocd flash $(BIN)

.PHONY: all clean flash
