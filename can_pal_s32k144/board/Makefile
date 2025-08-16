# ==========================
# Module: board
# ==========================
CPU         := S32K144HFT0VLLT

# Compiler mặc định nếu chạy độc lập
CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size

# Thư mục gốc (nếu chạy độc lập thì ROOT_DIR = thư mục cha)
ROOT_DIR ?= ..

# Thư mục board và build
BOARD_DIR := $(ROOT_DIR)/board
BUILD_DIR ?= $(ROOT_DIR)/build

# Include path mặc định (có thể override từ Makefile chính)
INCLUDES := \
  -I$(ROOT_DIR)/board \
  -I$(ROOT_DIR)/SDK/platform/devices \
  -I$(ROOT_DIR)/SDK/platform/drivers/inc \
  -I$(ROOT_DIR)/SDK/platform/drivers/src/adc \
  -I$(ROOT_DIR)/SDK/platform/pal/inc \
  -I$(ROOT_DIR)/SDK/rtos/osif \


# CFLAGS mặc định (có thể override)
CFLAGS  := -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS  += -DCPU_$(CPU) -DS32K14x_SERIES
ASFLAGS := $(CFLAGS)

# Danh sách file .c trong module này
BOARD_SRCS := \
	clock_config.c \
	peripherals_adc_config_1.c \
	peripherals_adc_pal_1.c \
	peripherals_can_pal1.c \
	peripherals_osif1.c \
	pin_mux.c

# Danh sách object file (nằm trong build/board/)
BOARD_OBJS := $(patsubst %.c,$(BUILD_DIR)/board/%.o,$(BOARD_SRCS))

# Export cho Makefile chính
MODULE_OBJS += $(BOARD_OBJS)
export MODULE_OBJS

# Rule build object
$(BUILD_DIR)/board/%.o: $(BOARD_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build tất cả object của module này
all: $(BOARD_OBJS)

# Dọn dẹp file build của module này
clean_board:
	rm -rf $(BUILD_DIR)/board
