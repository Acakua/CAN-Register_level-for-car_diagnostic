# ==========================
# Module: SDK
# ==========================
CPU         := S32K144HFT0VLLT

# Compiler mặc định nếu chạy độc lập
CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size

# Thư mục gốc (nếu chạy độc lập thì ROOT_DIR = thư mục cha)
ROOT_DIR ?= ..

# Thư mục SDK và build
SDK_DIR   := $(ROOT_DIR)/SDK
BUILD_DIR ?= $(ROOT_DIR)/build

# Include path mặc định (có thể override từ Makefile chính)
INCLUDES := \
  -I$(ROOT_DIR)/SDK/platform/devices \
  -I$(ROOT_DIR)/SDK/platform/drivers/inc \

# CFLAGS mặc định (có thể override)
CFLAGS  := -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS  += -DCPU_$(CPU) -DS32K14x_SERIES
ASFLAGS := $(CFLAGS)

# Danh sách file .c trong module SDK (thêm theo nhu cầu thực tế)
SDK_SRCS := \
    platform/devices/S32K144/startup/system_S32K144.c \
    platform/devices/startup.c \
    platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c \
    platform/drivers/src/interrupt/interrupt_manager.c \
    platform/drivers/src/pins/pins_driver.c \
    platform/drivers/src/pins/pins_port_hw_access.c

# Danh sách object file (nằm trong build/SDK/)
SDK_OBJS := $(patsubst %,$(BUILD_DIR)/SDK/%,$(SDK_SRCS:.c=.o))

# Export cho Makefile chính
MODULE_OBJS += $(SDK_OBJS)
export MODULE_OBJS

# Rule build object
$(BUILD_DIR)/SDK/%.o: $(SDK_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build tất cả object của module này
all: $(SDK_OBJS)

# Dọn dẹp file build của module này
clean_SDK:
	rm -rf $(BUILD_DIR)/SDK
