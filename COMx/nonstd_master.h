#if 0
#ifndef _NONSTD_MASTER_H
#define _NONSTD_MASTER_H
#include "type.h"

void modbus_init(void);
re_error_enum modbus_creat(char* dev_name_ptr, u32 baud_rate);
re_error_enum modbus_write_mul_line(u8 start_line, u8 line_num, u8 val);
re_error_enum modbus_read_line(u8 start_line, u8 line_num, u8* val_num_ptr, u8 *val_ptr);
re_error_enum modbus_write_line(u8 line_id, u8 val);
re_error_enum modbus_write_mul_reg(u16 start_reg, u16 reg_num, s16 val);
re_error_enum modbus_write_reg(u16 reg_id, u16 val);
re_error_enum modbus_read_binary(u16 start_reg, u16 reg_num, u8* val_num_ptr,
        u8 *val_ptr);
re_error_enum modbus_read_hold_reg(u16 start_reg, u16 reg_num, u8* val_num_ptr,
        u8 *val_ptr);
void modbus_dev_switch(u8 addr);
#endif
#endif
