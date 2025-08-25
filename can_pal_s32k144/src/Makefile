# ==========================
# Module: src
# ==========================
CPU         := S32K144HFT0VLLT
# Compiler mặc định nếu chạy độc lập
CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size

# Thư mục gốc (nếu chạy độc lập thì ROOT_DIR = thư mục cha)
ROOT_DIR ?= ..

# Thư mục src và build
SRC_DIR := $(ROOT_DIR)/src
BUILD_DIR ?= $(ROOT_DIR)/build

# Include path mặc định (có thể override từ Makefile chính)
INCLUDES = \
  -I$(ROOT_DIR)/src \
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

# Danh sách file .c trong module này (chỉ tên file, không kèm src/)
SRC_SRCS := \
	main.c \
	FlexCan.c \
	adc.c \
	uds.c

# Danh sách object file (nằm trong build/src/)
SRC_OBJS := $(patsubst %.c,$(BUILD_DIR)/src/%.o,$(SRC_SRCS))

# Export cho Makefile chính
MODULE_OBJS += $(SRC_OBJS)
export MODULE_OBJS

# Rule build object
$(BUILD_DIR)/src/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build tất cả object của module này
all: $(SRC_OBJS)

# Dọn dẹp file build của module này
clean_src:
	rm -rf $(BUILD_DIR)/src

