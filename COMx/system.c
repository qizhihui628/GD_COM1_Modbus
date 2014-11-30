#include "system.h"
#include <regex.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

void get_current_time(int *year_ptr, int *month_ptr, int *day_ptr,
        int* weekday_ptr, int *hour_ptr, int *minitue_ptr, int *second_ptr)
{
	time_t t;
	struct tm *nowtime;

	time(&t);
	nowtime = localtime(&t);
	*year_ptr = nowtime->tm_year + 1900;
	*month_ptr = nowtime->tm_mon + 1;
	*day_ptr = nowtime->tm_mday;
	*hour_ptr = nowtime->tm_hour;
	*minitue_ptr = nowtime->tm_min;
	*second_ptr = nowtime->tm_sec;
	*weekday_ptr = nowtime->tm_wday;
}

int match_time_key(char *string, char* pattern)
{
	regex_t reg;
	regmatch_t pm[1];       //pattern matches 0-9
	const size_t nmatch = 1;  //The size of array pm[]


	if (regcomp(&reg, pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0)
	{
		printf("error:creat regular expression: %s failed\r\n", pattern);
		return 1;
	}

	if (regexec(&reg, string, nmatch, pm, 0) != REG_NOERROR)
	{
		printf("error:%s match time format %s: failed\r\n", string, pattern);
		return 1;
	}

	printf("match time success %s pattern:%s\r\n", string, pattern);

	return 0;
}
