#ifndef _NONSTD_MASTER_H
#define _NONSTD_MASTER_H
#include "type.h"

void nonstd_init(void);
re_error_enum nonstd_creat(char* dev_name_ptr, u32 baud_rate);
re_error_enum nonstd_get_status(u8* run_status, u8* run_mode, u16* run_temp);
re_error_enum nonstd_on_ff(u8 value);
re_error_enum nonstd_ctrl_mode(u8 value);
void nonstd_dev_switch(u8 addr, u8 air_no);
#endif

