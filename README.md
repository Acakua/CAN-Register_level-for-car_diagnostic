# 🚗 Bare-metal Project for S32K144 – Manual Makefile Setup

Dự án này sử dụng Makefile thủ công (không dùng IDE) để build firmware cho vi điều khiển **S32K144**. Hướng dẫn này dành cho người mới, chi tiết từng bước để có thể build thành công từ terminal.

---

## ✅ 1. Cài đặt Môi Trường

### 🔧 Toolchain ARM (bắt buộc)
- Tải tại: [https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- Có thể sử dụng lệnh trong terminal: sudo apt install gcc-arm-none-eabi
- Sau khi cài đặt, thêm vào PATH:
    - `arm-none-eabi-gcc`

### 🔧 GNU Make
- Với Windows:
    - Dùng `Git Bash` (đã có sẵn `make`)
    - Hoặc cài `make` qua `MSYS2`

### 📁 Cấu trúc dự án
```bash
CAN-Register_level-for-car_diagnostic/
├── Makefile                    # Tập tin build chính
├── build/                      # Output folder (tự động tạo)
├── CMSIS/
│   ├── system_S32K144.c
│   └── startup.c
│   └── core_cm4.h              #file này tìm trên github
│   └── device_register.h
├── Project_Settings/
│   └── Startup_Code/
│       └── startup_S32K144.S
├── linker/
│   └── S32K144_64_flash.ld
├── src/
│   ├── main.c
│   ├── FlexCAN.c
│   ├── NVM.c
│   ├── DTC.c
│   ├── UDS.c
│   └── UDS_Server.c
├── board/
│   ├── pin_mux.c
│   └── clock_config.c
├── SDK/
│   └── platform/
│       ├── drivers/
│       │   └── src/
│       │       ├── clock/
│       │       └── pins/
│       └── devices/S32K144/
```
## 🛠 2. Hướng Dẫn Viết Makefile Cho S32K144 (GCC Toolchain)

---
### 📁 Cấu Trúc Tổng Quan

```makefile
TARGET = main                   #Tên chương trình đầu ra
CPU = S32K144HFT0VLLT           #Tên đầy đủ của vi điều khiển

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

#Các cờ biên dịch (compile flags)
CFLAGS = -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS += -DCPU_$(CPU)
CFLAGS += -DS32K14x_SERIES

ASFLAGS = $(CFLAGS)             #Dùng để biên dịch file assembly .S

BUILD_DIR = build
```
### 📂 Khai Báo Include Path
Khai báo đầy đủ các đường dẫn tới các file .h đã sử dụng trong src
```
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
```
### 📜 Linker Script
File .ld khai báo vừng nhớ (flash, RAM).
```
LDSCRIPT = ./SDK/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld
```
### 🧾 Danh Sách File Nguồn
Liệt kê toàn bộ .c và .S cần build. Nếu cần thêm file thì phải thêm đường dẫn tới file đó
Lưu ý: phải căn lề bằng Tab chứ không phải dấu cách.
```
SRCS = \
  src/main.c \
  src/FlexCan.c \
  src/adc.c \
  src/uds.c \
  board/clock_config.c \
  board/pin_mux.c \
  Project_Settings/Startup_Code/startup_s32K144.S \
  SDK/platform/devices/S32K144/startup/system_S32K144.c \
  SDK/platform/devices/startup.c \
  SDK/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c \
  SDK/platform/drivers/src/interrupt/interrupt_manager.c \
  SDK/platform/drivers/src/pins/pins_driver.c \
  SDK/platform/drivers/src/pins/pins_port_hw_access.c
```
### 🧱 Tạo Danh Sách File .o Từ .c/.S
```
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c,$(SRCS)))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(filter %.S,$(SRCS)))
```
### 📂 File đầu ra 
```
# File đầu ra
ELF = $(BUILD_DIR)/$(TARGET).elf
BIN = $(BUILD_DIR)/$(TARGET).bin
HEX = $(BUILD_DIR)/$(TARGET).hex
```
### 🔧 Luật Build Chính
#### Build tất cả
```
all: $(ELF) $(HEX) $(BIN)
$(SIZE) $(ELF)
```

#### Build ELF
```
$(ELF): $(OBJS)
$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -T $(LDSCRIPT) -Wl,--gc-sections -lnosys -o $@
```

#### Tạo HEX và BIN
```
$(HEX): $(ELF)
$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
$(OBJCOPY) -O binary $< $@
```
#### Build file .c
```
$(BUILD_DIR)/%.o: %.c
@mkdir -p $(dir $@)
$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
```

#### Build file .S
```
$(BUILD_DIR)/%.o: %.S
@mkdir -p $(dir $@)
$(CC) $(ASFLAGS) $(INCLUDES) -c $< -o $@
```
#### Dọn dẹp
```
clean:
rm -rf $(BUILD_DIR)
```
#### Nạp chương trình
```
flash: $(BIN)
pyocd flash $(BIN)
```
#### Luật PHONY
```
.PHONY: all clean flash
```
### 💻 Các lệnh sử dụng
Nên xoá thư mục build trước khi build project
```
make         # Build firmware
make clean   # Xóa thư mục build/
make flash   # Nạp file bin vào board
```

