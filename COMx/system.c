#include "system.h"
#include <stdio.h>
#include <regex.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "sql_op.h"

#define MAX_BUF_SIZE 20
#define SUBSLEN 10
#define SYSTEM_CONFIG_DB "./DataBase/JNSysHost.db3"
#define SYSTEM_JW_TABLE "tblStore"
#define SYSTEM_STORE "StoreName"
#define SYSTEM_STORE_NAME "全家"
#define LONGITUDE "Longitude"
#define LATITUDE "Latitude"
suntime_struct suntime = {0, 0, 0, 0, "", ""};
static int match_record_flag = 0;
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

int match_time_key(char *string, char* pattern, char* sub_ptr)
{
	regex_t reg;
	regmatch_t pm[SUBSLEN];       //pattern matches 0-9
	size_t len;  //The size of array pm[]
	int i;

	if (regcomp(&reg, pattern, REG_EXTENDED | REG_ICASE) != 0)
	{
		printf("error:creat regular expression: %s failed\r\n", pattern);
		return 1;
	}

	if (regexec(&reg, string, (size_t)SUBSLEN, pm, 0) != REG_NOERROR)
	{
		printf("error:%s match time format %s: failed\r\n", string, pattern);
		regfree(&reg);
		return 1;
	}

	printf("match time success %s pattern:%s\r\n", string, pattern);
	if (sub_ptr != NULL)
	{
		for(i = 1; i <= reg.re_nsub; i++)
		{
			len = pm[i].rm_eo - pm[i].rm_so;

			printf("renum :%d, st %d, num %d \r\n", reg.re_nsub, pm[i].rm_so, len);
			memcpy (sub_ptr, (string + pm[i].rm_so), len);
			*(sub_ptr + len) = '\0';
			printf("sub str: %s\r\n", sub_ptr);
		}
	}

	regfree(&reg);
	return 0;
}

static int enter_record_get_jw_info(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	int i;

	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_name[i], LONGITUDE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			suntime.longitude = atof(column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], LATITUDE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			suntime.latitude = atof(column_value[i]);
			match_record_flag++;
		}

	}
	return 0;

}

static void select_store(void)
{
	sql_select_where_equal(SYSTEM_STORE, SYSTEM_STORE_NAME);
}
#if 1


static double RAD = 180.0 * 3600 /M_PI;
static double richu;
static double midDayTime;
static double dawnTime;
static double jd;
static double wd;

/*************************
     * 儒略日的计算
     *
     * @param y 年
     *
     * @param M 月
     *
     * @param d 日
     *
     * @param h 小时
     *
     * @param m 分
     *
     * @param s秒
     *
     * @return int
***************************/
static double timeToDouble(int y, int M, double d)
{
	double B = 0;
	double jd = 0;

	B = -13;
	jd = floor(365.25 * (y + 4716)) + floor(30.60001 * (M + 1)) + B + d
	        - 1524.5;
	return jd;
}

static void doubleToStr(double time, char *str)
{
	double t;
	int h, m, s;

	t = time + 0.5;
	t = (t - (int) t) * 24;
	h = (int) t;
	t = (t - h) * 60;
	m = (int) t;
	t = (t - m) * 60;
	s = (int) t;
	sprintf(str, "%02d:%02d:%02d", h, m, s);
}

/****************************
 * @param t 儒略世纪数
 *
 * @return 太阳黄经
 *****************************/
static double sunHJ(double t)
{
	double j;
	t = t + (32.0 * (t + 1.8) * (t + 1.8) - 20) / 86400.0 / 36525.0;
	// 儒略世纪年数,力学时
	j = 48950621.66 + 6283319653.318 * t + 53 * t * t - 994
	        + 334166 * cos(4.669257 + 628.307585 * t)
	        + 3489 * cos(4.6261 + 1256.61517 * t)
	        + 2060.6 * cos(2.67823 + 628.307585 * t) * t;
	return (j / 10000000);
}

static double mod(double num1, double num2)
{
	num2 = fabs(num2);
	// 只是取决于Num1的符号
	return num1 >= 0 ?
	        (num1 - (floor(num1 / num2)) * num2) :
	        ((floor(fabs(num1) / num2)) * num2 - fabs(num1));
}
/********************************
 * 保证角度∈(-π,π)
 *
 * @param ag
 * @return ag
 ***********************************/
static double degree(double ag)
{
	ag = mod(ag, 2 * M_PI);
	if (ag <= -M_PI)
	{
		ag = ag + 2 * M_PI;
	}
	else if (ag > M_PI)
	{
		ag = ag - 2 * M_PI;
	}
	return ag;
}

/***********************************
 *
 * @param date  儒略日平午
 *
 * @param lo    地理经度
 *
 * @param la    地理纬度
 *
 * @param tz    时区
 *
 * @return 太阳升起时间
 *************************************/
double sunRiseTime(double date, double lo, double la, double tz)
{
	double t, j, sinJ, cosJ, gst, E, a, D, cosH0, cosH1, H0, H1, H;
	date = date - tz;
	// 太阳黄经以及它的正余弦值
	t = date / 36525;
	j = sunHJ(t);
	// 太阳黄经以及它的正余弦值
	sinJ = sin(j);
	cosJ = cos(j);
	// 其中2*M_PI*(0.7790572732640 + 1.00273781191135448*jd)恒星时（子午圈位置）
	gst = 2 * M_PI * (0.779057273264 + 1.00273781191135 * date)
	        + (0.014506 + 4612.15739966 * t + 1.39667721 * t * t) / RAD;
	E = (84381.406 - 46.836769 * t) / RAD; // 黄赤交角
	a = atan2(sinJ * cos(E), cosJ); // '太阳赤经
	D = asin(sin(E) * sinJ); // 太阳赤纬
	cosH0 = (sin(-50 * 60 / RAD) - sin(la) * sin(D)) / (cos(la) * cos(D)); // 日出的时角计算，地平线下50分
	cosH1 = (sin(-6 * 3600 / RAD) - sin(la) * sin(D)) / (cos(la) * cos(D)); // 天亮的时角计算，地平线下6度，若为航海请改为地平线下12度
	// 严格应当区分极昼极夜，本程序不算
	if (cosH0 >= 1 || cosH0 <= -1)
	{
		return -0.5;        // 极昼
	}
	H0 = -acos(cosH0); // 升点时角（日出）若去掉负号 就是降点时角，也可以利用中天和升点计算
	H1 = -acos(cosH1);
	H = gst - lo - a; // 太阳时角
	midDayTime = date - degree(H) / M_PI / 2 + tz; // 中天时间
	dawnTime = date - degree(H - H1) / M_PI / 2 + tz; // 天亮时间
	return date - degree(H - H0) / M_PI / 2 + tz; // 日出时间，函数返回值
}

void getSunTime(void)
{
	char timestr[20];
	int year;
	int month;
	double day;
	int hour;
	int min;
	int sec;
	int week;
	int day_int;
	int tz;
	int i;
	int result;
	double jd_degrees;
	double jd_seconds;
	double wd_degrees;
	double wd_seconds;

	result = sql_select(SYSTEM_CONFIG_DB, SYSTEM_JW_TABLE, select_store,
	        enter_record_get_jw_info, NULL);
	if (result != 0)
	{
		printf("error: db: %s,table: %s disable or do not exist\r\n",
		SYSTEM_CONFIG_DB, SYSTEM_JW_TABLE);
		return;
	}

	if (match_record_flag == 2)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		printf("error: db: %s,table: %s disable or do not exist\r\n",
		SYSTEM_CONFIG_DB, SYSTEM_JW_TABLE);
		return;
	}
	jd_degrees = floor(suntime.longitude);
	jd_seconds = (suntime.longitude - jd_degrees) * 60;
	wd_degrees = floor(suntime.latitude);
	wd_seconds = (suntime.latitude - wd_degrees) * 60;
	printf("jd_degrees=%f, jd_seconds=%f\r\n", jd_degrees, jd_seconds);
	printf("wd_degrees=%f, wd_seconds=%f\r\n", wd_degrees, wd_seconds);
	//上海东经121度29分，北纬31度14分
	//jd_degrees = 121;
	//jd_seconds = 28;
	//wd_degrees = 31;
	//wd_seconds = 14;
	//乌鲁木齐东经87度35分，北纬43度48分
	//jd_degrees=87;
	//jd_seconds=35;
	//wd_degrees=43;
	//wd_seconds=48;
	//广州 E113°16' N23°06'
	//jd_degrees=113;
	//jd_seconds=16;
	//wd_degrees=23;
	//wd_seconds=6;

	tz = 8;

	//step 1
	jd = -(jd_degrees + jd_seconds / 60) / 180 * M_PI;
	wd = (wd_degrees + wd_seconds / 60) / 180 * M_PI;

	//step 2
	get_current_time(&year, &month, &day_int, &week, &hour, &min, &sec);
	day = day_int;
	richu = timeToDouble(year, month, day) - 2451544.5;
	printf("mjd:%f\r\n", richu);
	printf("year=%d,month=%d,day=%f\r\n", year, month, day);
	for (i = 0; i < 10; i++)
	{
		richu = sunRiseTime(richu, jd, wd, tz / 24.0);    // 逐步逼近法算10次
	}

	doubleToStr(richu, timestr);
	printf("日出时间 %s\r\n", timestr);
	strcpy(suntime.timestr_sunrise, timestr);
	doubleToStr(midDayTime + midDayTime - richu, timestr);
	printf("日落时间 %s\r\n", timestr);
	printf("richu:%f,midDayTime:%f\r\n", richu, midDayTime);
	strcpy(suntime.timestr_sunset, timestr);
	doubleToStr(midDayTime, timestr);
	printf("中天时间 %s\r\n", timestr);

	doubleToStr(dawnTime, timestr);
	printf("天亮时间 %s\r\n", timestr);

	doubleToStr(midDayTime + midDayTime - dawnTime, timestr);
	printf("天黑时间 %s\r\n", timestr);

}

#endif
