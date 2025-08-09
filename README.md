# 🚗 Bare-metal Project for S32K144 – Manual Makefile Setup

Dự án này sử dụng Makefile thủ công (không dùng IDE) để build firmware cho vi điều khiển **S32K144**. Hướng dẫn này dành cho người mới, chi tiết từng bước để có thể build thành công từ terminal.

---

## ✅ 1. Cài đặt Môi Trường

### 🔧 Toolchain ARM (bắt buộc)
- Tải tại: https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-win32.exe
- Sau khi cài đặt, thêm vào PATH
### 🔧 Cài đặt GnuWin32 (bắt buộc)
- Tải tại: https://zenlayer.dl.sourceforge.net/project/gnuwin32/make/3.81/make-3.81.exe?viasf=1
- Sau khi cài đặt thêm vào PATH
### 🔧 Cài đặt Mingw64 (bắt buộc)
- Tải tại: https://objects.githubusercontent.com/github-production-release-asset-2e65be/80988227/82422b48-9b91-44f3-b0cb-6472f27b1bf0?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=releaseassetproduction%2F20250721%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20250721T021638Z&X-Amz-Expires=1800&X-Amz-Signature=33861da4f0893378aca8faca00eb9ea7462d15730c8b1e68b2f4620a46cbfed6&X-Amz-SignedHeaders=host&response-content-disposition=attachment%3B%20filename%3Dmsys2-x86_64-20250622.exe&response-content-type=application%2Foctet-stream
- Sau khi cài đặt thêm vào PATH
#### Hướng dẫn thêm đường dẫn vào PATH:
1. Trong thanh tìm kiếm tìm **Edit the system enviroment variables**  
   ![img.png](Tutorial%20Image%2Fimg.png)

2. Chọn Environment Variables  
   ![Enviroment Variables.PNG](Tutorial%20Image%2FEnviroment%20Variables.PNG)

3. Chọn Path  
   ![Path.PNG](Tutorial%20Image%2FPath.PNG)

4. Chọn New  
   ![New.PNG](Tutorial%20Image%2FNew.PNG)

5. Sau đó thêm đường dẫn tới thư mục có chứa
- File **bin** của toolchain
- Bên trong file **bin** của GnuWin32
- Bên trong file **bin** của mingw64  
  ![AddPath.PNG](Tutorial%20Image%2FAddPath.PNG)

6. Kiểm tra lại
- arm-none-eabi-gcc --version  
  ![Arm_check.PNG](Tutorial%20Image%2FArm_check.PNG)
- make --version  
  ![Make_check.PNG](Tutorial%20Image%2FMake_check.PNG)
- gcc --version  
  ![Gcc_check.PNG](Tutorial%20Image%2FGcc_check.PNG)
**Nếu kết quả hiển thị tương tự như trên tức là bạn đã thành công cài đặt môi trường**


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
BUILD_DIR = build               #vị trí các file sau khi compile
```
### 🔧 Các cờ biên dịch (compile flags)
- gcc: Compile và link
- objcopy: Chuyển ELF sang định dạng .bin, .hex
- size: In ra kích thước chương trình
```
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size
```
### 🔧 Compiler flags
- Trình biên dịch sử dụng Cortex-M4(S32K144)
- ffreestanding: báo rằng đây là môi trường nhúng, không có thư viện tiêu chuẩn (libc).
```
CFLAGS  := -mcpu=cortex-m4 -mthumb -Wall -O0 -g -std=c11 -ffreestanding
CFLAGS  += -DCPU_$(CPU) -DS32K14x_SERIES
ASFLAGS := $(CFLAGS)      #Dùng để biên dịch file assembly .S

```
### 📂 Khai Báo Base Path
Khai báo các biến đường dẫn gốc giúp quản lý tập trung
```
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

```
### 📂 Khai Báo Include Path
Khai báo đầy đủ các đường dẫn tới các file .h đã sử dụng trong src
```
# Danh sách thư mục chứa header
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

# thêm -I trước từng đường dẫn — kết quả cho compiler biết nơi tìm header.
INCLUDES := $(addprefix -I,$(INCLUDE_DIRS))

```
### 📜 Linker Script
File .ld khai báo vừng nhớ (flash, RAM).
```
LDSCRIPT := $(SDK_DIR)/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld
```
### 🧾 Danh Sách File Nguồn
Liệt kê toàn bộ .c và .S cần build. Nếu cần thêm file thì phải thêm đường dẫn tới file đó
Lưu ý: phải căn lề bằng Tab chứ không phải dấu cách.
```
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
# Không cần quét lại file nếu không thêm hoặc xoá file
SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.[cS]))

```
### 🧱 Tạo Danh Sách File .o Từ .c/.S
```
OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c,$(SRCS)))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(filter %.S,$(SRCS)))
```
### 📂 File đầu ra 
```
# File đầu ra
ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
HEX := $(BUILD_DIR)/$(TARGET).hex
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
**Trừ khi thay thêm hoặc xoá file trong project nếu không thì không cần phải clean trước khi build để tránh dẫn tới việc phải quét lại toàn bộ các file .c và .S**
```
make         # Build firmware
make clean   # Xóa thư mục build
```

