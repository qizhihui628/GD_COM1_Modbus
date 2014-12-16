
#ifndef _DEV_DAIKIN_MOD_H
#define _DEV_DAIKIN_MOD_H
#include "type.h"
#define MAX_DEV_NUM_EACH_MODULE 3
typedef struct{
	char module_table[MAX_DEV_NUM_EACH_MODULE][20];
	int module_addr;
}air_module_struct;
re_error_enum dev_temp_module_monitor(void);
#endif
