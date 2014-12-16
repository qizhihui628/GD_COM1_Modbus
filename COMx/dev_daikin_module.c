#include "nonstd_master.h"
#include "dev_daikin_module.h"
#include "system.h"
#include "sql_op.h"
#include <stdio.h>

#define NONSTD_CONFIG_DB "./DataBase/Nonstd_Config.db"
#define AIR_REGISTER_TABLE "DAIKIN_Register"
#define AIR_ADDR "Address"
#define AIR_STATUS "Status"
#define AIR_TABLE "Table_Name"

#define AIR_DB "./DataBase/DAIKIN.db"

#define AIR_DAY_TYPE "Date_Type"
#define AIR_STATUS_TABLE "DAIKIN_Status"
#define AIR_ADDRESS "DAIKIN_Address"
#define AIR_RUNNING_STATE "Running_State"
#define AIR_RUNNING_MODE "Running_Mode"
#define AIR_RUNNING_TEMPERATURE "Running_Temperature"

#define MAX_AIR_MODULE_NUMBER 8

#define TIME_START_PATTERN "^Start_Time([0-9]+)$"
#define TIME_END_PATTERN "^End_Time([0-9]+)$"
#define TIME_MODE_PATTERN "^Mode([0-9]+)$"
#define TIME_TEMPERATURE_PATTERN "^Temperature([0-9]+)$"

#define MODE_DEFROST_PATTERN "defrost"
#define MODE_ON_PATTERN "normal"
#define MODE_OFF_PATTERN "off"

#define TIME_PATTERN_NUM 4
#define AIR_STATUS_NUM 2

#define AIR_SET_REG_ADDR 0x0376
#define AIR_RD_SET_REG_ADDR 0x0600
#define AIR_FROST_REG_ADDR 0x0201
#define AIR_ON_OFF_REG_ADDR 0x0200
#define AIR_SET_REG_NUM 1

#define AIR_RD_STATUS_ADDR 0x0200
#define AIR_RD_STATUS_BITNUM 0x0008
#define AIR_FROST_ADDR 0x0002
#define AIR_ON_OFF_ADDR 0x0001


static u8 match_record_flag = 0;
static u8 match_air_module_num = 0;
static u8 current_module_id = 0;

static air_module_struct air_module_array[MAX_AIR_MODULE_NUMBER];

static void dev_air_select_spec_date(void);
static void dev_air_select_date(void);
static void dev_air_select_date(void);
static void dev_air_select_modue(void);

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

static void dev_air_select_modue(void)
{
	sql_select_where_equal(AIR_STATUS, "enable");
}

static void dev_air_select_module_st(void)
{
	char cur_addr[4];
	sprintf((char*) cur_addr, "%d",
	        air_module_array[current_module_id].module_addr);
	sql_select_where_equal(AIR_ADDRESS, cur_addr);
}

static void dev_air_update_module_st(void *value_ptr)
{
	char cur_temp[5] = {0};
	if((*(int *) value_ptr & AIR_ON_OFF_ADDR)== 0)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_OFF_PATTERN);
		match_record_flag = 1;
		return;
	}
	if ((*(int *) value_ptr & AIR_FROST_ADDR)== AIR_FROST_ADDR)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_DEFROST_PATTERN);
		match_record_flag = 1;
	}
	else if ((*(int *) value_ptr & AIR_ON_OFF_ADDR)== AIR_ON_OFF_ADDR)
	{
		sql_select_where_equal(AIR_RUNNING_MODE, MODE_ON_PATTERN);
		match_record_flag = 1;
	}
	sql_add(",");
	sprintf((char*) cur_temp, "%d", (*((int *) value_ptr+1)));
	printf("set val: %d\r\n",(*((int *) value_ptr+1)));
	sql_select_where_equal(AIR_RUNNING_TEMPERATURE, cur_temp);
}

static re_error_enum dev_air_status_update(int *value_ptr)
{
	u8 val_num = 1;
	u8 val_buf[5] = { 0 };
	re_error_enum re_val;

	re_val = modbus_read_binary(AIR_RD_STATUS_ADDR, AIR_RD_STATUS_BITNUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		return re_val;
	}

	*value_ptr = val_buf[0];

	return RE_SUCCESS;
}

static re_error_enum dev_air_temperature_update(int *value_ptr)
{
	u8 val_num = 2;
	u8 val_buf[5] = { 0 };
	re_error_enum re_val;

	re_val = modbus_read_hold_reg(AIR_RD_SET_REG_ADDR, AIR_SET_REG_NUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		return re_val;
	}
	if (val_num == 2)
	{
		printf("val1: %d, val2: %d\r\n", val_buf[0], val_buf[1]);
		*value_ptr = (((u16)val_buf[0] << 8) | (u16)val_buf[1])/10;
		printf("val1: %d, val2: %d, val: %d\r\n", val_buf[0], val_buf[1], *value_ptr);
	}
	else
	{
		printf("error %d: serial read line failed\n", re_val);
		return re_val;
	}

	return RE_SUCCESS;
}

static re_error_enum dev_air_ctrl_val_set(char* set_val)
{
	re_error_enum re_val = RE_SUCCESS;

	re_val = modbus_write_mul_reg(AIR_SET_REG_ADDR, AIR_SET_REG_NUM, (atoi(set_val)*10));
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

	if (strcmp(set_mode, MODE_DEFROST_PATTERN) == 0)
	{
		re_val = modbus_write_reg(AIR_FROST_REG_ADDR, 1);
	}
	else if (strcmp(set_mode, MODE_ON_PATTERN) == 0)
	{
		re_val = modbus_write_reg(AIR_FROST_REG_ADDR, 0);
		re_val |= modbus_write_reg(AIR_ON_OFF_REG_ADDR, 1);

	}
	else if (strcmp(set_mode, MODE_OFF_PATTERN) == 0)
	{
		re_val = modbus_write_reg(AIR_ON_OFF_REG_ADDR, 0);
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
	    printf(" %d column is %s, value is %s\r\n", column_no+3,column_name[column_no+3], column_value[column_no+3]);
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

static int enter_record_get_value(void * para, int n_column, char ** column_value,
        char ** column_name)
{
	int i;
	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_name[i], AIR_RUNNING_MODE) == 0)
		{

			dev_air_status_update((int *) para);
			match_record_flag++;
		}
		if (strcmp(column_name[i], AIR_RUNNING_TEMPERATURE) == 0)
		{

			dev_air_temperature_update((int *) para+1);
			match_record_flag++;
		}


	}
	return 0;
}

static re_error_enum dev_air_module_init(void)
{
	int result;
	memset(air_module_array, 0, sizeof(air_module_array));
	match_air_module_num = 0;
	current_module_id = 0;

	result = sql_select(NONSTD_CONFIG_DB, AIR_REGISTER_TABLE,
	        dev_air_select_modue, enter_record_get_module_info, NULL);
	if (result != 0)
	{
		printf("error: db: %s,air: %s disable or do not exist\r\n",
				NONSTD_CONFIG_DB, AIR_REGISTER_TABLE);
		return RE_OP_FAIL;
	}

	if (match_record_flag == 2)
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
	return RE_SUCCESS;
}

static re_error_enum dev_air_module_switch(u8 air_mod_id)
{
	int value_buf[AIR_STATUS_NUM] = {0};
	int result;
	if (air_mod_id > match_air_module_num)
	{
		printf("error: air module: %d disable or do not exist\r\n",
		        air_mod_id);
		return RE_OP_FAIL;
	}
	if (air_module_array[air_mod_id].module_addr
	        == 0|| strcmp(air_module_array[air_mod_id].module_table, "") == 0)
	{
		printf("error: air module: %d disable or do not exist\r\n",
		        air_mod_id);
		return RE_OP_FAIL;
	}
	current_module_id = air_mod_id;
	modbus_dev_switch(air_module_array[air_mod_id].module_addr);
	match_record_flag = 0;
	/*control the air according to the config in responding table*/
	result = sql_select(AIR_DB, air_module_array[air_mod_id].module_table,
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
		result = sql_select(AIR_DB,
		        air_module_array[air_mod_id].module_table,
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

	/*get the value to status table*/
	result = sql_select(AIR_DB, AIR_STATUS_TABLE,
	        dev_air_select_module_st, enter_record_get_value, value_buf);
	if (result != 0)
	{
		printf("error: db: %s,air: %s disable or do not exist\r\n", AIR_DB,
		AIR_STATUS_TABLE);
		return RE_OP_FAIL;
	}
	if (match_record_flag == AIR_STATUS_NUM)
	{
		match_record_flag = 0;

	}
	else
	{
		match_record_flag = 0;
		printf("error: db: %s,table: %s configure error\r\n", AIR_DB,
		AIR_STATUS_TABLE);
		return RE_OP_FAIL;
	}

	/*Update value in status table*/
	result = sql_update(AIR_DB, AIR_STATUS_TABLE,
	        dev_air_select_module_st, dev_air_update_module_st, value_buf);
	if (result != 0)
	{
		match_record_flag = 0;
		printf("error: db: %s,air: %s disable or do not exist\r\n", AIR_DB,
		AIR_STATUS_TABLE);
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
		AIR_STATUS_TABLE);
		return RE_OP_FAIL;
	}
	return RE_SUCCESS;

}

re_error_enum dev_air_module_monitor(void)
{
	re_error_enum re_val = RE_SUCCESS;
	int i;

	re_val = dev_air_module_init();
	if (re_val != RE_SUCCESS)
	{
		printf("error: air module init failed\r\n");
		return re_val;
	}
	for (i = 0; i < match_air_module_num; i++)
	{
		re_val = dev_air_module_switch(i);
		if (re_val != RE_SUCCESS)
		{
			printf("error: air module %d: operation failed\r\n", i);
			return re_val;
		}
	}

	return RE_SUCCESS;
}

