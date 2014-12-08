#ifndef _SYSTEM_H
#define _SYSTEM_H
#include "type.h"
#define MAX_TIME_PERIOD 12
void  get_current_time(int *year_ptr, int *month_ptr, int *day_ptr, int* weekday_ptr, int *hour_ptr, int *minitue_ptr, int *second_ptr);
int match_time_key(char *string, char* pattern, char** sub_pptr);
#endif
