#ifndef _SYSTEM_H
#define _SYSTEM_H
#include "type.h"
#define MAX_TIME_PERIOD 12
#define COMMUNICATION_ERROR "communication_error"
#define NONE ""
#define FORCE_CONTROL "forever"
#define SUN_CONTROL "sun_control"
#define NO_EXPECTION "no exception"

typedef struct
{
	float sunsise_delay;
	float sunset_delay;
	char timestr_sunrise[20];
	char timestr_sunset[20];
}suntime_struct;

void  get_current_time(int *year_ptr, int *month_ptr, int *day_ptr, int* weekday_ptr, int *hour_ptr, int *minitue_ptr, int *second_ptr);
int match_time_key(char *string, char* pattern, char* sub_ptr);
void getSunTime(double jd_degrees, double jd_seconds, double wd_degrees, double wd_seconds);
#endif
