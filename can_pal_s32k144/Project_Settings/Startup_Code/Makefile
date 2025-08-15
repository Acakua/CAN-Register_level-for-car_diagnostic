BUILD_DIR ?= build
MODULE_NAME := Project_Settings/Startup_Code

SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(patsubst %.c,$(BUILD_DIR)/$(MODULE_NAME)/%.o,$(SRC_FILES))

CC      ?= arm-none-eabi-gcc
CFLAGS  ?= -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS  += -I../../CMSIS \
           -I../../src \
           -I../../board \
           -I../../SDK \
           -I../../Project_Settings/Startup_Code \
           -I../../board

.PHONY: all clean

all: $(OBJ_FILES)

$(BUILD_DIR)/$(MODULE_NAME)/%.o: %.c
	@mkdir -p $(BUILD_DIR)/$(MODULE_NAME)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/$(MODULE_NAME)
