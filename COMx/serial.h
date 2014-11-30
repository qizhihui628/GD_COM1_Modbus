#ifndef _SERIAL_H
#define _SERIAL_H
#include "type.h"
void serial_init(void);
re_error_enum serial_creat(char* dev_name_ptr, u32 baud_rate, int* fd_ptr);
re_error_enum serial_write(int fd, u8 *buf_ptr, u8 buf_size);
re_error_enum serial_read(int fd, u8 max_size, u8 *buf_ptr, u8* buf_size_ptr);
#endif
