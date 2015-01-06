#include "modbus_master.h"
#include "dev_light_mod.h"
#include "system.h"
#include "sql_op.h"
#include <stdio.h>
#include "pthread.h"

extern pthread_mutex_t thread_mutex;
#define MODBUS_CONFIG_DB "./DataBase/ModBus_Config.db"
#define LIGHT_REGISTER_TABLE "Light_Register"
#define LIGHT_ADDR "Address"
#define LIGHT_STATUS "Status"
#define LIGHT_TABLE "Table_Name"

#define LIGHT_DB "./DataBase/Light.db"

#define LIGHT_DAY_TYPE "Date_Type"
#define LIGHT_STATUS_TABLE "Light_Status"
#define LIGHT_ADDRESS "Light_Address"
#define LIGHT_RUNNING_STATE "Running_State"

#define LIGHT_NUMBER 4
#define MAX_LIGHT_MODULE_NUMBER 12
#define TIME_KEY_PATTERN "^[ONF]+_Time[0-9]+$"

static u8 match_record_flag = 0;
static u8 match_light_module_num = 0;
static u8 current_module_id = 0;

static light_module_struct light_module_array[MAX_LIGHT_MODULE_NUMBER];

static void dev_light_select_spec_date(void);
static void dev_light_select_date(void);
static void dev_light_select_date(void);
static void dev_light_select_modue(void);

static void dev_light_select_spec_date(void)
{
	int month, day, other;
	int year;
	char cur_date[10];

	get_current_time(&year, &month, &day, &other, &other, &other, &other);
	sprintf((char*) cur_date, "%4d-%02d-%02d", year, month, day);
	sql_select_where_equal(LIGHT_DAY_TYPE, cur_date);

}

static void dev_light_select_date(void)
{
	int weekday, other;

	get_current_time(&other, &other, &other, &weekday, &other, &other, &other);

	if (weekday == 0 || weekday == 6)
	{
		sql_select_where_equal(LIGHT_DAY_TYPE, "weekend");
	}
	else
	{
		sql_select_where_equal(LIGHT_DAY_TYPE, "workday");
	}
}

static void dev_light_select_modue(void)
{
	sql_select_where_equal(LIGHT_STATUS, "enable");
}

static void dev_light_select_module_st(void)
{
	char cur_addr[4];
	sprintf((char*) cur_addr, "%d",
	        light_module_array[current_module_id].module_addr);
	sql_select_where_equal(LIGHT_ADDRESS, cur_addr);
}

static void dev_light_update_module_st(void *value_ptr)
{
	if (*(int *) value_ptr == 0)
	{
		sql_select_where_equal(LIGHT_RUNNING_STATE, "OFF");
	}
	else
	{
		sql_select_where_equal(LIGHT_RUNNING_STATE, "ON");
	}
	match_record_flag = 1;

}

static re_error_enum dev_light_status_update(u8 *value_ptr)
{
	u8 val_num;
	u8 val_buf[5] = { 0 };
	re_error_enum re_val;

	re_val = modbus_read_line(0, LIGHT_NUMBER, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		return re_val;
	}

	*value_ptr = val_buf[0];

	return RE_SUCCESS;
}

static re_error_enum dev_light_ctrl_val_set(u8 set_val)
{
	re_error_enum re_val;

	re_val = modbus_write_mul_line(0, LIGHT_NUMBER, set_val);
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
		if (strcmp(column_name[i], LIGHT_ADDR) == 0)
		{

			light_module_array[match_light_module_num].module_addr = atoi(
			        column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], LIGHT_TABLE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(light_module_array[match_light_module_num].module_table ,column_value[i]);
			match_record_flag++;
		}

	}
	match_light_module_num++;
	return 0;
}

static int enter_record_set_value(void * para, int n_column, char ** column_value,
        char ** column_name)
{
	int i;
	int switch_no = 0;
	int hour, minute, second, other;
	char cur_time[12];
	get_current_time(&other, &other, &other, &other, &hour, &minute, &second);
	sprintf((char*) cur_time, "%02d:%02d:%02d.000", hour, minute, second);
	for (i = 0; i < n_column; i++)
	{
		if (match_time_key(column_name[i], TIME_KEY_PATTERN, NULL) == 0)
		{
			printf("cur time is %s, config time is %s \r\n", cur_time, column_value[i]);
			if (column_value[i] == NULL)
			{
				match_record_flag = 1;
				return 0;
			}
			if (strcmp(cur_time, column_value[i]) < 0)
			{
				printf(" %s is %s,switch %d\r\n", column_name[i], column_value[i], switch_no);
				if ((switch_no % 2) == 0)
				{
					dev_light_ctrl_val_set(0);
					printf("set off\r\n");
				}
				else
				{
					dev_light_ctrl_val_set(0x0f);
					printf("set on\r\n");
				}
				match_record_flag = 1;
				return 0;
			}
			switch_no++;
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
		if (strcmp(column_name[i], LIGHT_RUNNING_STATE) == 0)
		{

			dev_light_status_update((int *) para);
			match_record_flag = 1;
		}
	}
	return 0;
}
static re_error_enum dev_light_module_init(void)
{
	int result;
	memset(light_module_array, 0, sizeof(light_module_array));
	match_light_module_num = 0;
	current_module_id = 0;

	result = sql_select(MODBUS_CONFIG_DB, LIGHT_REGISTER_TABLE,
	        dev_light_select_modue, enter_record_get_module_info, NULL);
	if (result != 0)
	{
		printf("error: db: %s,light: %s disable or do not exist\r\n",
		MODBUS_CONFIG_DB, LIGHT_REGISTER_TABLE);
		return RE_OP_FAIL;
	}

	if (match_record_flag == 2)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		match_light_module_num = 0;
		printf("error: db: %s,light: %s disable or do not exist\r\n",
		MODBUS_CONFIG_DB, LIGHT_REGISTER_TABLE);
		return RE_OP_FAIL;
	}
	return RE_SUCCESS;
}

static re_error_enum dev_light_module_switch(u8 light_mod_id)
{
	int value, result;
	if (light_mod_id > match_light_module_num)
	{
		printf("error: light module: %d disable or do not exist\r\n",
		        light_mod_id);
		return RE_OP_FAIL;
	}
	if (light_module_array[light_mod_id].module_addr
	        == 0|| strcmp(light_module_array[light_mod_id].module_table, "") == 0)
	{
		printf("error: light module: %d disable or do not exist\r\n",
		        light_mod_id);
		return RE_OP_FAIL;
	}
	current_module_id = light_mod_id;
	modbus_dev_switch(light_module_array[light_mod_id].module_addr);

	/*control the light according to the config in responding table*/
	result = sql_select(LIGHT_DB, light_module_array[light_mod_id].module_table,
	        dev_light_select_spec_date, enter_record_set_value, NULL);
	if (result != 0)
	{
		printf("error: db: %s,light: %s disable or do not exist\r\n", LIGHT_DB,
		        light_module_array[light_mod_id].module_table);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		result = sql_select(LIGHT_DB,
		        light_module_array[light_mod_id].module_table,
		        dev_light_select_date, enter_record_set_value, NULL);
		if (result != 0)
		{
			printf("error: db: %s,light: %s disable or do not exist\r\n",
			LIGHT_DB, light_module_array[light_mod_id].module_table);
			return RE_OP_FAIL;
		}
		if (match_record_flag)
		{
			match_record_flag = 0;
		}
		else
		{
			match_record_flag = 0;
			printf("error: db: %s,table: %s configure error\r\n", LIGHT_DB,
			        light_module_array[light_mod_id].module_table);
			return RE_OP_FAIL;
		}
	}

	/*get the value to status table*/
	result = sql_select(LIGHT_DB, LIGHT_STATUS_TABLE,
	        dev_light_select_module_st, enter_record_get_value, &value);
	if (result != 0)
	{
		printf("error: db: %s,light: %s disable or do not exist\r\n", LIGHT_DB,
		LIGHT_STATUS_TABLE);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		match_record_flag = 0;
		printf("error: db: %s,table: %s configure error\r\n", LIGHT_DB,
		LIGHT_STATUS_TABLE);
		return RE_OP_FAIL;
	}

	/*Update value in status table*/
	result = sql_update(LIGHT_DB, LIGHT_STATUS_TABLE,
	        dev_light_select_module_st, dev_light_update_module_st, &value);
	if (result != 0)
	{
		printf("error: db: %s,light: %s disable or do not exist\r\n", LIGHT_DB,
		LIGHT_STATUS_TABLE);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		match_record_flag = 0;
		printf("error: db: %s,table: %s set error\r\n", LIGHT_DB,
		LIGHT_STATUS_TABLE);
		return RE_OP_FAIL;
	}
	return RE_SUCCESS;

}

re_error_enum dev_light_module_monitor(void)
{
	re_error_enum re_val = RE_SUCCESS;
	int i;
	while(1)
	{
		printf("light module thread\r\n");

		re_val = dev_light_module_init();
		if (re_val != RE_SUCCESS)
		{
			printf("error: light module init failed\r\n");
			continue;
		}
		for (i = 0; i < match_light_module_num; i++)
		{
			pthread_mutex_lock(&thread_mutex);
			re_val = dev_light_module_switch(i);
			if (re_val != RE_SUCCESS)
			{
				printf("error: light module %d: operation failed\r\n", i);
			}
			pthread_mutex_unlock(&thread_mutex);
		}
		sleep(5);
	}
	return RE_SUCCESS;
}

