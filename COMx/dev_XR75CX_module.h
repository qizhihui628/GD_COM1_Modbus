
#ifndef _DEV_XR75CX_MOD_H
#define _DEV_XR75CX_MOD_H
#include "type.h"

typedef struct{
	char module_table[20];
	int module_addr;
}freeze_module_struct;
re_error_enum dev_freeze_module_monitor(void);
#endif
