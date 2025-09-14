################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/FlexCan.c \
../src/adc.c \
../src/main.c \
../src/nvm.c \
../src/uds.c 

OBJS += \
./src/FlexCan.o \
./src/adc.o \
./src/main.o \
./src/nvm.o \
./src/uds.o 

C_DEPS += \
./src/FlexCan.d \
./src/adc.d \
./src/main.d \
./src/nvm.d \
./src/uds.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/FlexCan.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


