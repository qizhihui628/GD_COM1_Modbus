#ifndef _DEV_POWER_MOD_H
#define _DEV_POWER_MOD_H
#include "type.h"

typedef struct{
	char module_table[20];
	int module_addr;
}power_module_struct;
re_error_enum dev_power_module_monitor(void);
#endif
