#ifndef _DEV_AIR_MOD_H
#define _DEV_AIR_MOD_H
#include "type.h"
#define MAX_DEV_NUM_EACH_MODULE 3
typedef struct{
	char air_id[MAX_DEV_NUM_EACH_MODULE][20];
	char module_table[20];
	char module_id[20];
	char module_type[20];
	int  module_addr;
}air_module_struct;
re_error_enum dev_air_module_monitor(void);
#endif
