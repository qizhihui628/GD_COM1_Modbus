################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../crc_check.c \
../dev_XR75CX_module.c \
../dev_air_module.c \
../dev_ligth_mod.c \
../dev_power_module.c \
../main.c \
../modbus_master.c \
../nonstd_master.c \
../serial.c \
../sql_op.c \
../system.c 

OBJS += \
./crc_check.o \
./dev_XR75CX_module.o \
./dev_air_module.o \
./dev_ligth_mod.o \
./dev_power_module.o \
./main.o \
./modbus_master.o \
./nonstd_master.o \
./serial.o \
./sql_op.o \
./system.o 

C_DEPS += \
./crc_check.d \
./dev_XR75CX_module.d \
./dev_air_module.d \
./dev_ligth_mod.d \
./dev_power_module.d \
./main.d \
./modbus_master.d \
./nonstd_master.d \
./serial.d \
./sql_op.d \
./system.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-linux-gcc -I/home/jeqi/git/GD_COM1_Modbus/COMx/lib -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


