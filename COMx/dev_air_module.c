#include "modbus_master.h"
#include "dev_air_module.h"
#include "system.h"
#include "sql_op.h"
#include <stdio.h>
#include "pthread.h"

extern pthread_mutex_t thread_mutex;
#define MODBUS_CONFIG_DB "./DataBase/ModBus_Config.db"
#define AIR_REGISTER_TABLE "AIR_Module_Register"
#define AIR_ADDR "Address"
#define AIR_STATUS "Status"
#define AIR_TABLE "Table_Name"
#define AIR_MODULE_TYPE "Module_Type"
#define AIR_ID "Air_ID"
#define MODULE_NAME "IR"
#define AIR_DB                   "./DataBase/Air.db"

#define AIR_DAY_TYPE             "Date_Type"

#define MAX_AIR_MODULE_NUMBER    8

#define TIME_START_PATTERN       "^Start_Time([0-9]+)$"
#define TIME_END_PATTERN         "^End_Time([0-9]+)$"
#define TIME_MODE_PATTERN        "^Mode([0-9]+)$"
#define TIME_TEMPERATURE_PATTERN "^Temperature([0-9]+)$"

#define AIR_TABLE_CONTROL 		 "_Control"
#define AIR_TABLE_CONFIG 		 "_Config"
#define AIR_TABLE_RECORD 		 "_Record"
#define AIR_TABLE_STATUS 		 "_Status"

#define AIR_RUNNING_STATE       "Running_State"
#define AIR_RUNNING_MODE        "Running_Mode"
#define AIR_RUNNING_TEMPERATURE "Running_Temperature"

#define AIR_CONFIG_MODE "Mode"
#define AIR_COMP_DELAY "Comp_Delay"
#define AIR_TEMP_DIFF "Temp_Differential"
#define AIR_SENSOR_CRCT "Sensor_Correct"
#define AIR_HIGH_ALARM "High_Alarm"
#define AIR_LOW_ALARM "Low_Alarm"
#define AIR_ALARM_DELAY "Alarm_Delay"

#define MODE_AUTO_PATTERN        "auto"
#define MODE_AIR_PATTERN         "air"
#define MODE_DEHUM_PATTERN       "dehumidity"
#define MODE_HEAT_PATTERN        "heat"
#define MODE_FREEZE_PATTERN      "freeze"
#define MODE_OFF_PATTERN         "off"

#define AIR_MODULE_PATTERN 		"Air_Module([0-9]+)"
#define AIR_MODULE_TYPE_PATTERN "IR-([0-9]+)"
#define TIME_PATTERN_NUM     4

#define AIR_SET_TYPE_REG         0x0002
#define AIR_SET_ON_OFF_REG       0x0004
#define AIR_SET_MODE_REG         0x0005
#define AIR_SET_TEMP_REG         0x0006

#define AIR_GET_REG          0x0000
#define AIR_GET_REG_NUM      0x0009



#define AIR_MODE_ADDR        0x0003
#define AIR_ON_OFF_ADDR      0x0001

static u8 match_record_flag = 0;
static u8 match_air_module_num = 0;
static u8 match_air_num = 0;
static u8 current_module_id = 0;
static u8 current_air_id = 0;
static u8 commnunication_error = 0;

static char cur_module_tbl_name[50] = "Module";

static air_module_struct air_module_array[MAX_AIR_MODULE_NUMBER];

static void dev_air_select_spec_date(void);
static void dev_air_select_date(void);
static void dev_air_select_module(void);

static char* module_table_name(char* type)
{
	strcpy(cur_module_tbl_name, "Module");
	strcat(cur_module_tbl_name, air_module_array[current_module_id].module_id);
	strcat(cur_module_tbl_name, type);
	return cur_module_tbl_name;
}

static void dev_air_select_spec_date(void)
{
	int month, day, other;
	int year;
	char cur_date[10];

	get_current_time(&year, &month, &day, &other, &other, &other, &other);
	sprintf((char*) cur_date, "%4d-%02d-%02d", year, month, day);
	sql_select_where_equal(AIR_ID, air_module_array[current_module_id].air_id[current_air_id]);
	sql_add(" and ");
	sql_select_where_equal(AIR_DAY_TYPE, cur_date);

}

static void dev_air_select_forever(void)
{

	sql_select_where_equal(AIR_ID, air_module_array[current_module_id].air_id[current_air_id]);
	sql_add(" and ");
	sql_select_where_equal(AIR_DAY_TYPE, FORCE_CONTROL);

}

static void dev_air_select_date(void)
{
	int weekday, other;

	get_current_time(&other, &other, &other, &weekday, &other, &other, &other);
	sql_select_where_equal(AIR_ID, air_module_array[current_module_id].air_id[current_air_id]);
	sql_add(" and ");
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
	sql_select_where_like(AIR_MODULE_TYPE, MODULE_NAME);
}

static void dev_air_select_air(void)
{
	sql_select_where_equal(AIR_STATUS, "enable");
}

static void dev_air_select_air_st(void)
{
	sql_select_where_equal(AIR_ID, air_module_array[current_module_id].air_id[current_air_id]);
}

static void dev_air_update_module_record(void *para)
{
	char cur_temp[5] = {0};
	u8 val_num, val_buf[AIR_GET_REG_NUM*2];
	re_error_enum re_val;

	re_val = modbus_read_hold_reg(AIR_GET_REG, AIR_GET_REG_NUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS || val_num != AIR_GET_REG_NUM*2)
	{
		printf("error %d: serial read line failed\n", re_val);
		commnunication_error = 1;
		sql_add("(null,");
		sql_add(air_module_array[current_module_id].air_id[current_air_id]);
		sql_add(",null,datetime('now','localtime'))");
		return;
	}

	sprintf((char*) cur_temp, "%d", val_buf[AIR_SET_TEMP_REG * 2 + 1]);
	printf("get val: %d\r\n",val_buf[AIR_SET_TEMP_REG * 2 + 1]);

	sql_add("(null,");
	sql_add(air_module_array[current_module_id].air_id[current_air_id]);
	sql_add(",");
	sql_add(cur_temp);
	sql_add(",datetime('now','localtime'))");

}

static void dev_air_update_module_st(void *para)
{
	char cur_temp[5] = {0};
	u8 val_num, val_buf[AIR_GET_REG_NUM*2];
	re_error_enum re_val;

	re_val = modbus_read_hold_reg(AIR_GET_REG, AIR_GET_REG_NUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS || val_num != AIR_GET_REG_NUM*2)
	{
		printf("error %d: modbus_get_status failed\n", re_val);
		commnunication_error = 1;
		sql_select_where_equal(AIR_RUNNING_MODE, NONE);
		sql_add(",");
		sql_select_where_equal(AIR_RUNNING_TEMPERATURE, NONE);
	}
	else
	{
		if ((val_buf[AIR_SET_ON_OFF_REG * 2 + 1]) == 0)
		{
			sql_select_where_equal(AIR_RUNNING_MODE, MODE_OFF_PATTERN);
			match_record_flag = 1;
		}
		else if ((val_buf[AIR_SET_ON_OFF_REG * 2 + 1]) == 0xff)
		{

			if (val_buf[AIR_SET_MODE_REG * 2 + 1] == 0)
			{
				sql_select_where_equal(AIR_RUNNING_MODE, MODE_AUTO_PATTERN);
				match_record_flag = 1;
			}
			else if (val_buf[AIR_SET_MODE_REG * 2 + 1] == 1)
			{
				sql_select_where_equal(AIR_RUNNING_MODE, MODE_FREEZE_PATTERN);
				match_record_flag = 1;
			}
			else if (val_buf[AIR_SET_MODE_REG * 2 + 1] == 2)
			{
				sql_select_where_equal(AIR_RUNNING_MODE, MODE_DEHUM_PATTERN);
				match_record_flag = 1;
			}
			else if (val_buf[AIR_SET_MODE_REG * 2 + 1] == 3)
			{
				sql_select_where_equal(AIR_RUNNING_MODE, MODE_AIR_PATTERN);
				match_record_flag = 1;
			}
			else if (val_buf[AIR_SET_MODE_REG * 2 + 1] == 4)
			{
				sql_select_where_equal(AIR_RUNNING_MODE, MODE_HEAT_PATTERN);
				match_record_flag = 1;
			}
			else
			{
				sql_select_where_equal(AIR_RUNNING_MODE, NONE);
				printf("error %d: get mode failed\n", re_val);
				commnunication_error = 1;
			}
		}
		else
		{
			sql_select_where_equal(AIR_RUNNING_MODE, NONE);
			printf("error %d: get mode failed\n", re_val);
			commnunication_error = 1;
		}

		sql_add(",");
		sprintf((char*) cur_temp, "%d", val_buf[AIR_SET_TEMP_REG * 2 + 1]);
		printf("get val: %d\r\n", val_buf[AIR_SET_TEMP_REG * 2 + 1]);
		sql_select_where_equal(AIR_RUNNING_TEMPERATURE, cur_temp);
	}
	sql_add(",");
	if (commnunication_error == 1)
	{
	    sql_select_where_equal(AIR_RUNNING_STATE, COMMUNICATION_ERROR);
	}
	else
	{
		sql_select_where_equal(AIR_RUNNING_STATE, NO_EXPECTION);
	}

}

static re_error_enum dev_air_ctrl_val_set(char* set_val)
{
	re_error_enum re_val = RE_SUCCESS;

	re_val = modbus_write_single_reg(AIR_SET_TYPE_REG, atoi(air_module_array[match_air_module_num].module_type));
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial write line failed\n", re_val);
		commnunication_error = 1;
		return re_val;
	}

	re_val = modbus_write_single_reg(AIR_SET_TEMP_REG, atoi(set_val));
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial write register failed\n", re_val);
		commnunication_error = 1;
		return re_val;
	}
	return re_val;
}

static re_error_enum dev_air_ctrl_mode_set(char* set_mode)
{
	re_error_enum re_val = RE_SUCCESS;

	re_val = modbus_write_single_reg(AIR_SET_TYPE_REG, atoi(air_module_array[match_air_module_num].module_type));
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial write line failed\n", re_val);
		commnunication_error = 1;
		return re_val;
	}

	if (strcmp(set_mode, MODE_OFF_PATTERN) == 0)
	{
		re_val = modbus_write_single_reg(AIR_SET_ON_OFF_REG, 0);
	}
	else if (strcmp(set_mode, MODE_AUTO_PATTERN) == 0)
	{
		re_val = modbus_write_single_reg(AIR_SET_ON_OFF_REG, 0xff);
		re_val |= modbus_write_single_reg(AIR_SET_MODE_REG, 0);

	}
	else if (strcmp(set_mode, MODE_FREEZE_PATTERN) == 0)
	{
		re_val = modbus_write_single_reg(AIR_SET_ON_OFF_REG, 0xff);
		re_val |= modbus_write_single_reg(AIR_SET_MODE_REG, 1);
	}
	else if (strcmp(set_mode, MODE_DEHUM_PATTERN) == 0)
	{
		re_val = modbus_write_single_reg(AIR_SET_ON_OFF_REG, 0xff);
		re_val |= modbus_write_single_reg(AIR_SET_MODE_REG, 2);
	}
	else if (strcmp(set_mode, MODE_AIR_PATTERN) == 0)
	{
		re_val = modbus_write_single_reg(AIR_SET_ON_OFF_REG, 0xff);
		re_val |= modbus_write_single_reg(AIR_SET_MODE_REG, 3);
	}
	else if (strcmp(set_mode, MODE_HEAT_PATTERN) == 0)
	{
		re_val = modbus_write_single_reg(AIR_SET_ON_OFF_REG, 0xff);
		re_val |= modbus_write_single_reg(AIR_SET_MODE_REG, 4);
	}
	else
	{
		printf("error %d: invalid key %s\n", re_val, set_mode);
		return RE_OP_FAIL;
	}

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial write line failed\n", re_val);
		commnunication_error = 1;
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
			match_time_key(column_value[i], AIR_MODULE_PATTERN, air_module_array[match_air_module_num].module_id);
			match_record_flag++;
		}
		if (strcmp(column_name[i], AIR_MODULE_TYPE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			match_time_key(column_value[i], AIR_MODULE_TYPE_PATTERN, air_module_array[match_air_module_num].module_type);
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
		if (strcmp(column_name[i], AIR_ID) == 0)
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
	    printf(" %d column is %s, value is %s\r\n", column_no+4, column_name[column_no+4], column_value[column_no+4]);
		if ((match_time_key(column_name[column_no+4], TIME_END_PATTERN, NULL) == 0)
			&& (strcmp(cur_time, column_value[column_no+4]) <= 0)
			&&(match_time_key(column_name[column_no+3], TIME_START_PATTERN, NULL) == 0)
			&& (strcmp(cur_time, column_value[column_no+3]) >= 0)
			&&(match_time_key(column_name[column_no+5], TIME_MODE_PATTERN, NULL) == 0)
			&&(match_time_key(column_name[column_no+6], TIME_TEMPERATURE_PATTERN, NULL) == 0))
			{

				printf(" %s is %s\r\n", column_name[column_no+5], column_value[column_no+4]);
				dev_air_ctrl_mode_set(column_value[column_no+5]);
				printf(" %s is %s\r\n", column_name[column_no+6], column_value[column_no+5]);
				dev_air_ctrl_val_set(column_value[column_no+6]);
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

	result = sql_select(MODBUS_CONFIG_DB, AIR_REGISTER_TABLE,
	        dev_air_select_module, enter_record_get_module_info, NULL);
	if (result != 0)
	{
		printf("error %d: db: %s,air: %s disable or do not exist\r\n",
				result, MODBUS_CONFIG_DB, AIR_REGISTER_TABLE);
		return RE_OP_FAIL;
	}
	if (match_record_flag%3 == 0)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		match_air_module_num = 0;
		printf("error: db: %s,air: %s disable or do not exist\r\n",
				MODBUS_CONFIG_DB, AIR_REGISTER_TABLE);
		return RE_OP_FAIL;
	}

	for (i = 0; i < match_air_module_num; i++)
	{
		current_module_id = i;
		result = sql_select(MODBUS_CONFIG_DB,
		        air_module_array[current_module_id].module_table,
		        dev_air_select_air, enter_record_get_air_info, NULL);
		if (result != 0)
		{
			printf("error: db: %s,air: %s disable or do not exist\r\n",
					MODBUS_CONFIG_DB, air_module_array[current_module_id].module_table);
			return RE_OP_FAIL;
		}

		if (match_record_flag)
		{
			match_record_flag = 0;
		}
		else
		{
			match_record_flag = 0;
			match_air_num = 0;
			printf("error: db: %s,air: %s disable or do not exist\r\n",
					MODBUS_CONFIG_DB, air_module_array[current_module_id].module_table);
			return RE_OP_FAIL;
		}
	}

	return RE_SUCCESS;
}

static re_error_enum dev_air_module_switch(u8 air_mod_id, u8 air_id)
{
	int result;

	if (air_mod_id > match_air_module_num || air_id > match_air_num)
	{
		printf("error: air module: %d or air :%d disable or do not exist\r\n",
		        air_mod_id, air_id);
		return RE_OP_FAIL;
	}

	if (air_module_array[air_mod_id].module_addr == 0
	        || strcmp(air_module_array[air_mod_id].module_table, "") == 0)
	{
		printf("error: air module: %d or air :%d disable or do not exist\r\n",
		        air_mod_id, air_id);
		return RE_OP_FAIL;
	}

	current_module_id = air_mod_id;
	current_air_id = air_id;
	commnunication_error = 0;
	modbus_dev_switch(air_module_array[air_mod_id].module_addr);

	match_record_flag = 0;

	/*froce control*/
	result = sql_select(AIR_DB, module_table_name(AIR_TABLE_CONTROL),
	        dev_air_select_forever, enter_record_set_value, NULL);
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
		match_record_flag = 0;
		/*control the air according to the config in responding table*/
		result = sql_select(AIR_DB, module_table_name(AIR_TABLE_CONTROL),
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
			result = sql_select(AIR_DB, module_table_name(AIR_TABLE_CONTROL),
			        dev_air_select_date, enter_record_set_value, NULL);
			if (result != 0)
			{
				printf("error: db: %s,air: %s disable or do not exist\r\n",
				AIR_DB, module_table_name(AIR_TABLE_CONTROL));
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
	}

	/*insert record in record table*/
	result = sql_insert(AIR_DB, module_table_name(AIR_TABLE_RECORD),
	        dev_air_update_module_record, NULL);
	if (result != 0)
	{
		printf("error: db: %s,power: %s disable or do not exist\r\n", AIR_DB,
		        module_table_name(AIR_TABLE_RECORD));
	}

	/*Update value in status table*/
	result = sql_update(AIR_DB, module_table_name(AIR_TABLE_STATUS),
	        dev_air_select_air_st, dev_air_update_module_st, NULL);
	if (result != 0)
	{
		match_record_flag = 0;
		printf("error: db: %s,air: %s disable or do not exist\r\n", AIR_DB,
		        module_table_name(AIR_TABLE_STATUS));
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
	int i;
	while (1)
	{
		re_val = dev_air_module_init();
		if (re_val != RE_SUCCESS)
		{
			printf("error: air module init failed\r\n");
			return re_val;
		}
		for (i = 0; i < match_air_module_num; i++)
		{
			if (match_air_num != 1)
			{
				printf("error: air module init failed\r\n");
				return re_val;
			}
			pthread_mutex_lock(&thread_mutex);
			re_val = dev_air_module_switch(i, 0);

			if (re_val != RE_SUCCESS)
			{
				printf("error: air module %d, air id 0 : operation failed\r\n",
				        i);
				return re_val;
			}
			pthread_mutex_unlock(&thread_mutex);

		}
		sleep(5);
	}

	return RE_SUCCESS;
}
