
#ifndef _DEV_DAIKIN_MOD_H
#define _DEV_DAIKIN_MOD_H
#include "type.h"

typedef struct{
	char module_table[20];
	int module_addr;
}air_module_struct;
re_error_enum dev_temp_module_monitor(void);
#endif
