#ifndef _DEV_LIGHT_MOD_H
#define _DEV_LIGHT_MOD_H
#include "type.h"

typedef enum{
	OFF,
	ON,
}light_switch_enum;

typedef struct{
	char module_table[20];
	int module_addr;
}light_module_struct;
re_error_enum dev_light_module_monitor(void);
#endif
