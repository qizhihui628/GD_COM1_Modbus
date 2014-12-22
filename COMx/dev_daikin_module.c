#include "nonstd_master.h"
#include "dev_daikin_module.h"
#include "system.h"
#include "sql_op.h"
#include <stdio.h>

#define NONSTD_CONFIG_DB "./DataBase/Air/COM2_Config.db"
#define AIR_REGISTER_TABLE "AIR_Module_Register"
#define AIR_ADDR "Address"
#define AIR_STATUS "Status"
#define AIR_TABLE "Table_Name"
#define AIR_MODULE_TYPE "Module_Type"
#define AIR_ID "Air_ID"

#define AIR_CHANNEL_ID "Channel_ID"
#define AIR_READ_TABLE "Read_Table"
#define AIR_WRITE_TABLE "Write_Table"

#define AIR_RUNNING_STATE       "Running_State"

#define AIR_RUNNING_MODE        "Running_Mode"

#define AIR_RUNNING_TEMPERATURE "Running_Temperature"

#define AIR_DB                   "./DataBase/Air/Air_DAIKIN.db"

#define AIR_DAY_TYPE             "Date_Type"

#define MAX_AIR_MODULE_NUMBER    8

#define TIME_START_PATTERN       "^Start_Time([0-9]+)$"
#define TIME_END_PATTERN         "^End_Time([0-9]+)$"
#define TIME_MODE_PATTERN        "^Mode([0-9]+)$"
#define TIME_TEMPERATURE_PATTERN "^Temperature([0-9]+)$"

#define AIR_TABLE_PATTERN 		 "^Module[0-9]+_Air([0-9]+)$"

#define MODE_FAN_PATTERN         "fan"
#define MODE_HEAT_PATTERN        "heat"
#define MODE_COOL_PATTERN        "cool"
#define MODE_OFF_PATTERN         "off"

#define TIME_PATTERN_NUM     4
#define AIR_STATUS_NUM       2

#define AIR_MODE_ADDR        0x0003
#define AIR_MODE_FAN         0x0000
#define AIR_MODE_HEAT        0x0001
#define AIR_MODE_COOL        0x0002
#define AIR_ON_OFF_ADDR      0x0001

static u8 match_record_flag = 0;
static u8 match_air_module_num = 0;
static u8 match_air_num = 0;
static u8 current_module_id = 0;
static u8 current_air_id = 0;

static air_module_struct air_module_array[MAX_AIR_MODULE_NUMBER];

static void dev_air_select_spec_date(void);
static void dev_air_select_date(void);
static void dev_air_select_module(void);

static void dev_air_select_spec_date(void)
{
	int month, day, other;
	int year;
	char cur_date[10];

	get_current_time(&year, &month, &day, &other, &other, &other, &other);
	sprintf((char*) cur_date, "%4d-%02d-%02d", year, month, day);
	sql_select_where_equal(AIR_DAY_TYPE, cur_date);

}

static void dev_air_select_date(void)
{
	int weekday, other;

	get_current_time(&other, &other, &other, &weekday, &other, &other, &other);

	if (weekday == 0 || weekday == 6)
	{
		sql_select_where_equal(AIR_DAY_TYPE, "weekend");
	}
	else
	{
		sql_select_where_equal(AIR_DAY_TYPE, "workday");
	}
}

static void dev_air_select_module(void)
{
	sql_select_where_equal(AIR_STATUS, "enable");
	sql_add(" and ");
	sql_select_where_equal(AIR_MODULE_TYPE, "DAIKIN");
}

static void dev_air_select_air(void)
{
	sql_select_where_equal(AIR_STATUS, "enable");
}

static void dev_air_select_air_st(void)
{
	sql_select_where_equal(AIR_ID, air_module_array[current_module_id].air_id[current_air_id]);
}

static void dev_air_update_module_st(void *para)
{
	char cur_temp[5] = {0};
	u8 status, mode;
	u16 temp;
	re_error_enum re_val;

	re_val = nonstd_get_status(&status, &mode, &temp);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: nonstd_get_status failed\n", re_val);
		return;
	}

	if ((status & AIR_ON_OFF_ADDR)== 0)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_OFF_PATTERN);
		match_record_flag = 1;
		return;
	}
	if ((mode & AIR_MODE_ADDR)== AIR_MODE_FAN)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_FAN_PATTERN);
		match_record_flag = 1;
	}
	else if ((mode & AIR_MODE_ADDR)== AIR_MODE_HEAT)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_HEAT_PATTERN);
		match_record_flag = 1;
	}
	else if ((mode & AIR_MODE_ADDR)== AIR_MODE_COOL)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_COOL_PATTERN);
		match_record_flag = 1;
	}
	sql_add(",");
	sprintf((char*) cur_temp, "%d", temp);
	printf("set val: %d\r\n",temp);
	sql_select_where_equal(AIR_RUNNING_TEMPERATURE, cur_temp);
}

static re_error_enum dev_air_ctrl_val_set(char* set_val)
{
	re_error_enum re_val = RE_SUCCESS;

	re_val = nonstd_set_temp(atoi(set_val), atoi(set_val));
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial write register failed\n", re_val);
		return re_val;
	}
	return re_val;
}

static re_error_enum dev_air_ctrl_mode_set(char* set_mode)
{
	re_error_enum re_val = RE_SUCCESS;

	if (strcmp(set_mode, MODE_FAN_PATTERN) == 0)
	{
		re_val = nonstd_on_ff(1);
		re_val |= nonstd_ctrl_mode(AIR_MODE_FAN);
	}
	else if (strcmp(set_mode, MODE_HEAT_PATTERN) == 0)
	{
		re_val = nonstd_on_ff(1);
		re_val |= nonstd_ctrl_mode(AIR_MODE_HEAT);

	}
	else if (strcmp(set_mode, MODE_COOL_PATTERN) == 0)
	{
		re_val = nonstd_on_ff(1);
		re_val |= nonstd_ctrl_mode(AIR_MODE_COOL);
	}
	else if (strcmp(set_mode, MODE_OFF_PATTERN) == 0)
	{
		re_val = nonstd_on_ff(0);
	}
	else
	{
		printf("error %d: invalid key %s\n", re_val, set_mode);
		return RE_OP_FAIL;
	}

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial write line failed\n", re_val);
		return re_val;
	}
	return re_val;
}

static int enter_record_get_module_info(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	int i;

	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_name[i], AIR_ADDR) == 0)
		{

			air_module_array[match_air_module_num].module_addr = atoi(
			        column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], AIR_TABLE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(air_module_array[match_air_module_num].module_table ,column_value[i]);
			match_record_flag++;
		}

	}
	match_air_module_num++;
	return 0;
}

static int enter_record_get_air_info(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	int i;

	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_name[i], AIR_READ_TABLE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(air_module_array[current_module_id].state_table, column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], AIR_WRITE_TABLE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(air_module_array[current_module_id].air_table[match_air_num], column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], AIR_CHANNEL_ID) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(air_module_array[current_module_id].air_id[match_air_num], column_value[i]);
			match_record_flag++;
		}

	}
	match_air_num++;
	return 0;
}

static int enter_record_set_value(void * para, int n_column, char ** column_value,
        char ** column_name)
{
	int i;
	int column_no = 0;
	int hour, minute, second, other;
	char cur_time[12];
	get_current_time(&other, &other, &other, &other, &hour, &minute, &second);
	sprintf((char*) cur_time, "%02d:%02d:%02d", hour, minute, second);
	printf("cur_time: %s\r\n", cur_time);

	for (i = 0; i < (n_column/TIME_PATTERN_NUM); i++)
	{
		column_no = i * 4;
	    printf(" %d column is %s, value is %s\r\n", column_no+3, column_name[column_no+3], column_value[column_no+3]);
		if ((match_time_key(column_name[column_no+3], TIME_END_PATTERN, NULL) == 0)
			&& (strcmp(cur_time, column_value[column_no+3]) <= 0)
			&&(match_time_key(column_name[column_no+2], TIME_START_PATTERN, NULL) == 0)
			&& (strcmp(cur_time, column_value[column_no+2]) >= 0)
			&&(match_time_key(column_name[column_no+4], TIME_MODE_PATTERN, NULL) == 0)
			&&(match_time_key(column_name[column_no+5], TIME_TEMPERATURE_PATTERN, NULL) == 0))
			{

				printf(" %s is %s\r\n", column_name[column_no+4], column_value[column_no+4]);
				dev_air_ctrl_mode_set(column_value[column_no+4]);
				printf(" %s is %s\r\n", column_name[column_no+5], column_value[column_no+5]);
				dev_air_ctrl_val_set(column_value[column_no+5]);
				match_record_flag = 1;
				return 0;
			}
	}

	return 0;
}

static re_error_enum dev_air_module_init(void)
{
	int result, i;
	memset(air_module_array, 0, sizeof(air_module_array));

	match_air_module_num = 0;
	current_module_id = 0;
	match_air_num = 0;

	result = sql_select(NONSTD_CONFIG_DB, AIR_REGISTER_TABLE,
	        dev_air_select_module, enter_record_get_module_info, NULL);
	if (result != 0)
	{
		printf("error %d: db: %s,air: %s disable or do not exist\r\n",
				result, NONSTD_CONFIG_DB, AIR_REGISTER_TABLE);
		return RE_OP_FAIL;
	}
	if (match_record_flag%2 == 0)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		match_air_module_num = 0;
		printf("error: db: %s,air: %s disable or do not exist\r\n",
				NONSTD_CONFIG_DB, AIR_REGISTER_TABLE);
		return RE_OP_FAIL;
	}

	for (i = 0; i < match_air_module_num; i++)
	{
		current_module_id = i;
		result = sql_select(NONSTD_CONFIG_DB,
		        air_module_array[current_module_id].module_table,
		        dev_air_select_air, enter_record_get_air_info, NULL);
		if (result != 0)
		{
			printf("error: db: %s,air: %s disable or do not exist\r\n",
			NONSTD_CONFIG_DB, air_module_array[current_module_id].module_table);
			return RE_OP_FAIL;
		}

		if (match_record_flag%3 == 0)
		{
			match_record_flag = 0;
		}
		else
		{
			match_record_flag = 0;
			match_air_num = 0;
			printf("error: db: %s,air: %s disable or do not exist\r\n",
					NONSTD_CONFIG_DB, air_module_array[current_module_id].module_table);
			return RE_OP_FAIL;
		}
	}

	return RE_SUCCESS;
}

static re_error_enum dev_air_module_switch(u8 air_mod_id, u8 air_id)
{
	int result;
	char * air_id_str;

	if (air_mod_id > match_air_module_num || air_id > match_air_num)
	{
		printf("error: air module: %d or air :%d disable or do not exist\r\n", air_mod_id, air_id);
		return RE_OP_FAIL;
	}

	if (air_module_array[air_mod_id].module_addr == 0
	        || strcmp(air_module_array[air_mod_id].module_table, "") == 0)
	{
		printf("error: air module: %d or air :%d disable or do not exist\r\n", air_mod_id, air_id);
		return RE_OP_FAIL;
	}

	current_module_id = air_mod_id;
	current_air_id = air_id;

	nonstd_dev_switch(air_module_array[air_mod_id].module_addr, (atoi(air_module_array[air_mod_id].air_id[air_id]) - 1));

	match_record_flag = 0;
	/*control the air according to the config in responding table*/
	result = sql_select(AIR_DB, air_module_array[air_mod_id].air_table[air_id],
	        dev_air_select_spec_date, enter_record_set_value, NULL);
	if (result != 0)
	{
		printf("error: db: %s,air: %s disable or do not exist\r\n", AIR_DB,
		        air_module_array[air_mod_id].module_table);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		result = sql_select(AIR_DB, air_module_array[air_mod_id].air_table[air_id],
		        dev_air_select_date, enter_record_set_value, NULL);
		if (result != 0)
		{
			printf("error: db: %s,air: %s disable or do not exist\r\n",
			AIR_DB, air_module_array[air_mod_id].module_table);
			return RE_OP_FAIL;
		}
		if (match_record_flag)
		{
			match_record_flag = 0;
		}
		else
		{
			match_record_flag = 0;
			printf("error: db: %s,table: %s configure error\r\n", AIR_DB,
			        air_module_array[air_mod_id].module_table);
			return RE_OP_FAIL;
		}
	}

	/*Update value in status table*/
	result = sql_update(AIR_DB, air_module_array[air_mod_id].state_table, dev_air_select_air_st,
	        dev_air_update_module_st, NULL);
	if (result != 0)
	{
		match_record_flag = 0;
		printf("error: db: %s,air: %s disable or do not exist\r\n", AIR_DB,
				air_module_array[air_mod_id].state_table);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		match_record_flag = 0;
		printf("error: db: %s,table: %s set error\r\n", AIR_DB,
				air_module_array[air_mod_id].module_table);
		return RE_OP_FAIL;
	}

	return RE_SUCCESS;

}

re_error_enum dev_air_module_monitor(void)
{
	re_error_enum re_val = RE_SUCCESS;
	int i, j;

	re_val = dev_air_module_init();
	if (re_val != RE_SUCCESS)
	{
		printf("error: air module init failed\r\n");
		return re_val;
	}
	for (i = 0; i < match_air_module_num; i++)
	{
		for (j = 0; j < match_air_num; j++)
		{
			re_val = dev_air_module_switch(i, j);
			if (re_val != RE_SUCCESS)
			{
				printf("error: air module %d, air id %d : operation failed\r\n", i, j);
				return re_val;
			}
		}
	}

	return RE_SUCCESS;
}
