#ifndef _CRC_CHECK_H
#define _CRC_CHECK_H

u16 CRC16_check(u8 *input_ptr, u8 length);
u16 checksum(u8 *input_ptr, u8 length);
re_error_enum hex2asc(u8 hex, u8* asc1, u8* asc2);
u8 asc2hex(u8 asc1, u8 asc2);
u8 asc2val(u8 asc);
u8 val2asc(u8 value);
#endif
