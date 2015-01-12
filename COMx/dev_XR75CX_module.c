#include "modbus_master.h"
#include "dev_XR75CX_module.h"
#include "system.h"
#include "sql_op.h"
#include <stdio.h>
#include "pthread.h"
extern pthread_mutex_t thread_mutex;

#define MODBUS_CONFIG_DB "./DataBase/ModBus_Config.db"
#define FREEZE_REGISTER_TABLE "Freezing_Register"
#define FREEZE_ADDR "Address"
#define FREEZE_STATUS "Status"
#define FREEZE_TABLE "Table_Name"
#define FREEZE_MODULE_TYPE "Module_Type"

#define MODULE_NAME "XR75CX"

#define FREEZE_DB "./DataBase/Freezing.db"

#define FREEZE_DAY_TYPE "Date_Type"

#define FREEZE_STATUS_TABLE "Freezing_Status"
#define FREEZE_ADDRESS "Freezing_Address"
#define FREEZE_RUNNING_STATE "Running_State"
#define FREEZE_RUNNING_MODE "Running_Mode"
#define FREEZE_RUNNING_TEMPERATURE "Running_Temperature"

#define FREEZE_CONFIG_TABLE "Freezing_Config"
#define FREEZE_COMP_DELAY "Comp_Delay"
#define FREEZE_TEMP_DIFF "Temp_Differential"
#define FREEZE_SENSOR_CRCT "Sensor_Correct"
#define FREEZE_HIGH_ALARM "High_Alarm"
#define FREEZE_LOW_ALARM "Low_Alarm"
#define FREEZE_ALARM_DELAY "Alarm_Delay"
#define FREEZE_REMIND_TEMP "Remind_Temp"
#define FREEZE_REMIND_DELAY "Remind_Delay"


#define ALARM_TEMP_SENSOR_ERR "temperature sensor error"
#define ALARM_FAN_SENSOR_ERR "fan sensor error"
#define ALARM_HIGH_ALARM_ERR "high temperature error"
#define ALARM_LOW_ALARM_ERR "low temperature error"
#define ALARM_EXTERN_ALARM_ERR "external error"
#define ALARM_SEVERE_ERR "severe error"
#define ALARM_EE_ALARM_ERR "EEPROM error"
#define ALARM_REMIND_ALARM_ERR "remind alarm error"


#define MAX_FREEZE_MODULE_NUMBER 8

#define TIME_START_PATTERN "^Start_Time([0-9]+)$"
#define TIME_END_PATTERN "^End_Time([0-9]+)$"
#define TIME_MODE_PATTERN "^Mode([0-9]+)$"
#define TIME_TEMPERATURE_PATTERN "^Temperature([0-9]+)$"

#define TIME_MODE1 "Mode1"

#define MODE_DEFROST_PATTERN "defrost"
#define MODE_ON_PATTERN "normal"
#define MODE_OFF_PATTERN "off"
#define MODE_LIGHT_ON_PATTERN "light"
#define MODE_COMPRESOR_PATTERN "compresor"
#define COMPRESOR_CLOSE_TEMP    50
#define TIME_PATTERN_NUM 4
#define FREEZE_STATUS_NUM 2

#define FREEZE_SET_REG_ADDR                 0x0376
#define FREEZE_RD_SET_REG_ADDR              0x0108
#define FREEZE_FROST_REG_ADDR               0x0201
#define FREEZE_SET_FROST_TIME_ADDR          0x0321
#define FREEZE_SET_FROST_TEMPERATURE_ADDR   0x031E

#define FREEZE_SET_COMP_DELAY_ADDR          0x030C
#define FREEZE_SET_TEMP_DIFF_ADDR           0x0301
#define FREEZE_SET_SENSOR_CR_ADDR           0x0304
#define FREEZE_SET_HIGH_ALARM_ADDR          0x0338
#define FREEZE_SET_LOW_ALARM_ADDR           0x0339
#define FREEZE_SET_ALARM_DELAY_ADDR         0x033b

#define FREEZE_ON_OFF_REG_ADDR 0x0200
#define FREEZE_ON_LIGHT_ADDR 0x021A
#define FREEZE_DEMIST_ADDR 0x0222
#define FREEZE_SET_REG_NUM 1

#define FREEZE_RD_STATUS_ADDR 	0x0200
#define FREEZE_RD_STATUS_BITNUM 0x0008
#define FREEZE_RD_ALARM_ADDR 	0x0208
#define FREEZE_RD_ALARM_BITNUM 	0x0020

#define FREEZE_FROST_ADDR  		0x0002
#define FREEZE_ON_OFF_ADDR 		0x0001

#define FREEZE_ERR_PB1_BIT 		0x01000000
#define FREEZE_ERR_PB2_BIT 		0x02000000
#define FREEZE_HIGH_VALUE_BIT 	0x10000000
#define FREEZE_LOW_VALUE_BIT  	0x20000000
#define FREEZE_EXTERNAL_BIT 	0x00010000
#define FREEZE_SEVERE_BIT 		0x00020000
#define FREEZE_EE_FAILURE_BIT 	0x00080000
#define FREEZE_REMIND_BIT 		0x00000100
#define FREEZE_COMMUNICATION_ERR_BIT  0x00000001

#define FREEZE_MAX_DEFROST_TIME 255

static u8 match_record_flag = 0;
static u8 match_freeze_module_num = 0;
static u8 current_module_id = 0;
static u8 commnunication_error = 0;
static char running_state_str[50];
static freeze_module_struct freeze_module_array[MAX_FREEZE_MODULE_NUMBER];
static char* cur_mode_array[MAX_FREEZE_MODULE_NUMBER]= {NULL};

static freeze_temp_ctrl_struct temp_info = {0xffff};


static void dev_freeze_select_spec_date(void);
static void dev_freeze_select_date(void);
static void dev_freeze_select_date(void);
static void dev_freeze_select_modue(void);

static void dev_freeze_select_spec_date(void)
{
	int month, day, other;
	int year;
	char cur_date[10];

	get_current_time(&year, &month, &day, &other, &other, &other, &other);
	sprintf((char*) cur_date, "%4d-%02d-%02d", year, month, day);
	sql_select_where_equal(FREEZE_DAY_TYPE, cur_date);

}

static void dev_freeze_select_date(void)
{
	int weekday, other;

	get_current_time(&other, &other, &other, &weekday, &other, &other, &other);

	if (weekday == 0 || weekday == 6)
	{
		sql_select_where_equal(FREEZE_DAY_TYPE, "weekend");
	}
	else
	{
		sql_select_where_equal(FREEZE_DAY_TYPE, "workday");
	}

}

static void dev_freeze_select_light_on_forever(void)
{

		sql_select_where_equal(FREEZE_DAY_TYPE, FORCE_CONTROL);
		sql_add(" and ");
		sql_select_where_equal(TIME_MODE1, MODE_LIGHT_ON_PATTERN);
}

static void dev_freeze_select_compresor_forever(void)
{

		sql_select_where_equal(FREEZE_DAY_TYPE, FORCE_CONTROL);
		sql_add(" and ");
		sql_select_where_equal(TIME_MODE1, MODE_COMPRESOR_PATTERN);
}

static void dev_freeze_select_modue(void)
{
	sql_select_where_equal(FREEZE_STATUS, "enable");
	sql_add(" and ");
	sql_select_where_equal(FREEZE_MODULE_TYPE, MODULE_NAME);
}

static void dev_freeze_select_module_st(void)
{
	char cur_addr[4];
	sprintf((char*) cur_addr, "%d",
	        freeze_module_array[current_module_id].module_addr);
	sql_select_where_equal(FREEZE_ADDRESS, cur_addr);
}

static re_error_enum dev_freeze_mode_update(int *value_ptr)
{
	u8 val_num = 1;
	u8 val_buf[5] = { 0 };
	re_error_enum re_val;

	re_val = modbus_read_binary(FREEZE_RD_STATUS_ADDR, FREEZE_RD_STATUS_BITNUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		commnunication_error = 1;
		return re_val;
	}

	*value_ptr = val_buf[0];

	return RE_SUCCESS;
}

static re_error_enum dev_freeze_alarm_update(int *value_ptr)
{
	u8 val_num = FREEZE_RD_ALARM_BITNUM/8;
	u8 val_buf[5] = { 0 };
	re_error_enum re_val = RE_SUCCESS;

	re_val = modbus_read_binary(FREEZE_RD_ALARM_ADDR, FREEZE_RD_ALARM_BITNUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line:%d failed\n", re_val, FREEZE_RD_ALARM_ADDR);
		commnunication_error = 1;
		return re_val;
	}
	if (val_num != FREEZE_RD_ALARM_BITNUM/8)
	{
		printf("error %d: serial read line:%d failed\n", re_val, FREEZE_RD_ALARM_ADDR);
		commnunication_error = 1;
		return RE_OP_FAIL;
	}
	printf("cur temp:%d, remind temp %d\r\n", temp_info.cur_temp, temp_info.remind_temp);
	val_buf[3] = 0;
	if (temp_info.remind_stop_count == 1)
	{
		val_buf[2] = 1;
	}
	else
	{
		val_buf[2] = 0;
	}

	if (temp_info.cur_temp > temp_info.remind_temp && temp_info.remind_start_count == 0)
	{
		temp_info.remind_start_count = 1;
		printf("start remind count\r\n");
	}
	if (temp_info.cur_temp <= temp_info.remind_temp && temp_info.remind_start_count == 0)
	{
		temp_info.remind_start_count = 2;
		printf("start normal count\r\n");
	}

	*value_ptr = ((u32)val_buf[0] << 24) | ((u32)val_buf[1] << 16) | ((u32)val_buf[2] << 8) | ((u32)val_buf[3]) ;
	printf("read alarm value is %d \r\n", *value_ptr);
	return RE_SUCCESS;
}

static re_error_enum dev_freeze_temperature_update(int *value_ptr)
{
	u8 val_num = 2;
	u8 val_buf[5] = { 0 };
	re_error_enum re_val;
	short tmp;

	re_val = modbus_read_hold_reg(FREEZE_RD_SET_REG_ADDR, FREEZE_SET_REG_NUM, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		commnunication_error = 1;
		return re_val;
	}
	if (val_num == 2)
	{
		printf("val1: %d, val2: %d\r\n", val_buf[0], val_buf[1]);

		tmp = ((short)(val_buf[0] << 8 | val_buf[1]))/10;
		*value_ptr = tmp;
		temp_info.cur_temp = *value_ptr;
		printf("val1: %d, val2: %d, tmp: %04x val: %d\r\n", val_buf[0], val_buf[1], tmp, *value_ptr);
	}
	else
	{
		printf("error %d: serial read line failed\n", re_val);
		commnunication_error = 1;
		return re_val;
	}

	return RE_SUCCESS;
}

static void dev_freeze_update_module_st(void *value_ptr)
{
	char cur_temp[5] =
	{ 0 };
	int mode, temp, state;

	re_error_enum re_val;

	re_val = dev_freeze_mode_update(&mode);
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read status failed\n", re_val);
		sql_select_where_equal(FREEZE_RUNNING_MODE, NONE);
	}
	else
	{
		if ((mode & FREEZE_ON_OFF_ADDR) == 0)
		{
			sql_select_where_equal(FREEZE_RUNNING_MODE, MODE_OFF_PATTERN);
		}
		else
		{
			if ((mode & FREEZE_FROST_ADDR) == FREEZE_FROST_ADDR)
			{
				sql_select_where_equal(FREEZE_RUNNING_MODE,
				        MODE_DEFROST_PATTERN);
			}
			else if ((mode & FREEZE_ON_OFF_ADDR) == FREEZE_ON_OFF_ADDR)
			{
				sql_select_where_equal(FREEZE_RUNNING_MODE, MODE_ON_PATTERN);
			}
		}

	}

	//running temperature update
	sql_add(",");
	re_val = dev_freeze_temperature_update(&temp);
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read temperature failed\n", re_val);
		sql_select_where_equal(FREEZE_RUNNING_TEMPERATURE, NONE);
	}
	else
	{
		sprintf((char*) cur_temp, "%d", temp);
		printf("set val: %d\r\n", temp);
		sql_select_where_equal(FREEZE_RUNNING_TEMPERATURE, cur_temp);
	}

	//running state update
	sql_add(",");
	strcpy(running_state_str, "");
	re_val = dev_freeze_alarm_update(&state);

	if (commnunication_error == 1)
	{
		state |= FREEZE_COMMUNICATION_ERR_BIT;
	}
	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read alarm failed\n", re_val);
		strcat(running_state_str, COMMUNICATION_ERROR);
	}
	else
	{
		if ((state & FREEZE_ERR_PB1_BIT) == FREEZE_ERR_PB1_BIT)
		{
			strcat(running_state_str, ALARM_TEMP_SENSOR_ERR);

		}
		if ((state & FREEZE_ERR_PB2_BIT) == FREEZE_ERR_PB2_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_FAN_SENSOR_ERR);
		}
		if ((state & FREEZE_HIGH_VALUE_BIT) == FREEZE_HIGH_VALUE_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_HIGH_ALARM_ERR);
		}
		if ((state & FREEZE_LOW_VALUE_BIT) == FREEZE_LOW_VALUE_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_LOW_ALARM_ERR);
		}
		if ((state & FREEZE_EXTERNAL_BIT) == FREEZE_EXTERNAL_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_EXTERN_ALARM_ERR);
		}
		if ((state & FREEZE_SEVERE_BIT) == FREEZE_SEVERE_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_SEVERE_ERR);
		}
		if ((state & FREEZE_EE_FAILURE_BIT) == FREEZE_EE_FAILURE_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_EE_ALARM_ERR);
		}
		if ((state & FREEZE_REMIND_BIT) == FREEZE_REMIND_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, ALARM_REMIND_ALARM_ERR);
		}
		if ((state & FREEZE_COMMUNICATION_ERR_BIT)
		        == FREEZE_COMMUNICATION_ERR_BIT)
		{
			strcat(running_state_str, "/");
			strcat(running_state_str, COMMUNICATION_ERROR);
		}
	}
	sql_select_where_equal(FREEZE_RUNNING_STATE, running_state_str);
}

static int dev_freeze_config_module(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	int i;
	re_error_enum re_val;
	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_value[i], "") == 0)
		{
			continue;
		}
		printf("key is %s, value is %s \r\n", column_name[i], column_value[i]);
		if (strcmp(column_name[i], FREEZE_COMP_DELAY) == 0)
		{

			re_val = modbus_write_mul_reg(FREEZE_SET_COMP_DELAY_ADDR,
			FREEZE_SET_REG_NUM, atoi(column_value[i]));
			if (re_val != RE_SUCCESS)
			{
				commnunication_error = 1;
				printf("error %d: serial set comp delay failed\n", re_val);
			}
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_TEMP_DIFF) == 0)
		{
			re_val = modbus_write_mul_reg(FREEZE_SET_TEMP_DIFF_ADDR,
			FREEZE_SET_REG_NUM, atoi(column_value[i]) * 10);
			if (re_val != RE_SUCCESS)
			{
				commnunication_error = 1;
				printf("error %d: serial set temp diff failed\n", re_val);
			}
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_SENSOR_CRCT) == 0)
		{

			re_val = modbus_write_mul_reg(FREEZE_SET_SENSOR_CR_ADDR,
			FREEZE_SET_REG_NUM, atoi(column_value[i]) * 10);
			if (re_val != RE_SUCCESS)
			{
				commnunication_error = 1;
				printf("error %d: serial set sensor correct failed\n", re_val);
			}
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_HIGH_ALARM) == 0)
		{
			re_val = modbus_write_mul_reg(FREEZE_SET_HIGH_ALARM_ADDR,
			FREEZE_SET_REG_NUM, atoi(column_value[i]) * 10);
			if (re_val != RE_SUCCESS)
			{
				commnunication_error = 1;
				printf("error %d: serial set high alarm failed\n", re_val);
			}
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_LOW_ALARM) == 0)
		{

			re_val = modbus_write_mul_reg(FREEZE_SET_LOW_ALARM_ADDR,
			FREEZE_SET_REG_NUM, atoi(column_value[i]) * 10);
			if (re_val != RE_SUCCESS)
			{
				commnunication_error = 1;
				printf("error %d: serial set low alarm failed\n", re_val);
			}
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_ALARM_DELAY) == 0)
		{
			re_val = modbus_write_mul_reg(FREEZE_SET_ALARM_DELAY_ADDR,
			FREEZE_SET_REG_NUM, atoi(column_value[i]));
			if (re_val != RE_SUCCESS)
			{
				commnunication_error = 1;
				printf("error %d: serial set alarm delay failed\n", re_val);

			}
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_REMIND_TEMP) == 0)
		{
			temp_info.remind_temp = atoi(column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_REMIND_DELAY) == 0)
		{
			temp_info.remind_delay = atoi(column_value[i]);
			match_record_flag++;
		}

	}
	return 0;
}

static int dev_freeze_force_light_on(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	re_error_enum re_val;
	match_record_flag++;
	printf("light on\r\n");
	re_val = modbus_write_reg(FREEZE_ON_LIGHT_ADDR, 1);
	if (re_val != RE_SUCCESS)
	{
		commnunication_error = 1;
		printf("error %d: serial write line failed\n", re_val);
		return 0;
	}


	return 0;

}

static int dev_freeze_force_compresor(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	re_error_enum re_val;
	match_record_flag++;
	printf("compresor force close\r\n");
	re_val = modbus_write_mul_reg(FREEZE_SET_REG_ADDR, FREEZE_SET_REG_NUM,
			        (COMPRESOR_CLOSE_TEMP * 10));
	if (re_val != RE_SUCCESS)
	{
		commnunication_error = 1;
		printf("error %d: serial write line failed\n", re_val);
		return 0;
	}
	match_record_flag++;

	return 0;

}

static re_error_enum dev_freeze_ctrl_mode_set(char* set_mode, char* set_value, int set_time)
{
	re_error_enum re_val = RE_SUCCESS;
	if (cur_mode_array[current_module_id] == NULL)
	{
		cur_mode_array[current_module_id] = "";
	}
	printf("cur id: %d, cur mode : %s\r\n", current_module_id,
	        cur_mode_array[current_module_id]);
	if ( strcmp(set_mode, MODE_DEFROST_PATTERN) == 0
	        && strcmp(cur_mode_array[current_module_id], MODE_DEFROST_PATTERN) != 0 )
	{
		printf("set defrost time :%d, ter temp: %s\r\n", set_time, set_value);
		if (set_time > FREEZE_MAX_DEFROST_TIME || set_time < 0)
		{
			printf("error: set exceed max defrost time limit:%d \r\n", FREEZE_MAX_DEFROST_TIME);
			return RE_OP_FAIL;
		}
		re_val = modbus_write_mul_reg(FREEZE_SET_FROST_TIME_ADDR,
		        FREEZE_SET_REG_NUM, set_time);
		re_val |= modbus_write_mul_reg(FREEZE_SET_FROST_TEMPERATURE_ADDR,
		        FREEZE_SET_REG_NUM, (atoi(set_value) * 10));
		re_val |= modbus_write_reg(FREEZE_FROST_REG_ADDR, 1);
		if (re_val != RE_SUCCESS)
		{
			commnunication_error = 1;
			printf("error %d: serial write line failed\n", re_val);
			return re_val;
		}
		cur_mode_array[current_module_id] = MODE_DEFROST_PATTERN;
	}
	else if (strcmp(set_mode, MODE_ON_PATTERN) == 0)
	{

		re_val = modbus_write_reg(FREEZE_ON_OFF_REG_ADDR, 1);
		re_val |= modbus_write_mul_reg(FREEZE_SET_REG_ADDR, FREEZE_SET_REG_NUM,
		        (atoi(set_value) * 10));
		if (re_val != RE_SUCCESS)
		{
			commnunication_error = 1;
			printf("error %d: serial write line failed\n", re_val);
			return re_val;
		}
		cur_mode_array[current_module_id] = MODE_ON_PATTERN;
	}
	else if (strcmp(set_mode, MODE_OFF_PATTERN) == 0)
	{
		re_val = modbus_write_reg(FREEZE_ON_OFF_REG_ADDR, 0);
		if (re_val != RE_SUCCESS)
		{
			commnunication_error = 1;
			printf("error %d: serial write line failed\n", re_val);
			return re_val;
		}
		cur_mode_array[current_module_id] = MODE_OFF_PATTERN;
	}

	return RE_SUCCESS;
}

static int enter_record_get_module_info(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	int i;

	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_name[i], FREEZE_ADDR) == 0)
		{

			freeze_module_array[match_freeze_module_num].module_addr = atoi(
			        column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], FREEZE_TABLE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(freeze_module_array[match_freeze_module_num].module_table ,column_value[i]);
			match_record_flag++;
		}

	}
	match_freeze_module_num++;
	return 0;
}

static int enter_record_set_value(void * para, int n_column, char ** column_value,
        char ** column_name)
{
	int i;
	int column_no = 0;
	int hour, minute, second, other;
	int hour_st, minute_st,hour_end,minute_end;
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
				sscanf(column_value[column_no+3], "%d:%d:%d", &hour_end, &minute_end, &other);
				sscanf(column_value[column_no+2], "%d:%d:%d", &hour_st, &minute_st, &other);
				minute = (hour_end - hour_st) * 60 + (minute_end - minute_st);

				printf(" %s is %s\r\n", column_name[column_no+4], column_value[column_no+4]);
				if (column_value[column_no+4] == NULL || column_value[column_no+5] == NULL)
				{
					printf("error:invalid key value\r\n");
					return 1;
				}
				dev_freeze_ctrl_mode_set(column_value[column_no+4], column_value[column_no+5], minute);
				printf(" %s is %s\r\n", column_name[column_no+5], column_value[column_no+5]);
				match_record_flag = 1;
				return 0;
			}
	}

	return 0;
}

static re_error_enum dev_freeze_module_init(void)
{
	int result;
	memset(freeze_module_array, 0, sizeof(freeze_module_array));
	match_freeze_module_num = 0;
	current_module_id = 0;

	result = sql_select(MODBUS_CONFIG_DB, FREEZE_REGISTER_TABLE,
	        dev_freeze_select_modue, enter_record_get_module_info, NULL);
	if (result != 0)
	{
		printf("error: db: %s,freeze: %s disable or do not exist\r\n",
		MODBUS_CONFIG_DB, FREEZE_REGISTER_TABLE);
		return RE_OP_FAIL;
	}

	if (match_record_flag == 2)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		match_freeze_module_num = 0;
		printf("error: db: %s,freeze: %s disable or do not exist\r\n",
		MODBUS_CONFIG_DB, FREEZE_REGISTER_TABLE);
		return RE_OP_FAIL;
	}
	return RE_SUCCESS;
}

static re_error_enum dev_freeze_module_switch(u8 freeze_mod_id)
{
	int result;

	if (freeze_mod_id > match_freeze_module_num)
	{
		printf("error: freeze module: %d disable or do not exist\r\n",
		        freeze_mod_id);
		return RE_OP_FAIL;
	}
	if (freeze_module_array[freeze_mod_id].module_addr == 0
	        || strcmp(freeze_module_array[freeze_mod_id].module_table, "") == 0)
	{
		printf("error: freeze module: %d disable or do not exist\r\n",
		        freeze_mod_id);
		return RE_OP_FAIL;
	}
	current_module_id = freeze_mod_id;
	modbus_dev_switch(freeze_module_array[freeze_mod_id].module_addr);
	match_record_flag = 0;
	commnunication_error = 0;

	/*force control the freeze */
	result = sql_select(FREEZE_DB,
	        freeze_module_array[freeze_mod_id].module_table,
	        dev_freeze_select_light_on_forever, dev_freeze_force_light_on,
	        NULL);
	if (result != 0)
	{
		printf("error: db: %s,freeze: %s disable or do not exist\r\n",
		FREEZE_DB, freeze_module_array[freeze_mod_id].module_table);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		match_record_flag = 0;
		printf("light off\r\n");
		result = modbus_write_reg(FREEZE_ON_LIGHT_ADDR, 0);
		if (result != 0)
		{
			commnunication_error = 1;
			printf("error: force freeze light off failed \r\n");
		}
	}

	result = sql_select(FREEZE_DB,
	        freeze_module_array[freeze_mod_id].module_table,
	        dev_freeze_select_compresor_forever, dev_freeze_force_compresor,
	        NULL);
	if (result != 0)
	{
		printf("error: db: %s,freeze: %s disable or do not exist\r\n",
		FREEZE_DB, freeze_module_array[freeze_mod_id].module_table);
	}
	if (match_record_flag)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		printf("compresor recovery\r\n");
		/*control the freeze according time*/
			result = sql_select(FREEZE_DB,
			        freeze_module_array[freeze_mod_id].module_table,
			        dev_freeze_select_spec_date, enter_record_set_value, NULL);
			if (result != 0)
			{
				printf("error: db: %s,freeze: %s disable or do not exist\r\n",
				        FREEZE_DB, freeze_module_array[freeze_mod_id].module_table);
				return RE_OP_FAIL;
			}
			if (match_record_flag)
			{
				match_record_flag = 0;

			}
			else
			{
				result = sql_select(FREEZE_DB,
				        freeze_module_array[freeze_mod_id].module_table,
				        dev_freeze_select_date, enter_record_set_value, NULL);
				if (result != 0)
				{
					printf("error: db: %s,freeze: %s disable or do not exist\r\n",
					FREEZE_DB, freeze_module_array[freeze_mod_id].module_table);
					return RE_OP_FAIL;
				}
				if (match_record_flag)
				{
					match_record_flag = 0;
				}
				else
				{
					match_record_flag = 0;
					printf("error: db: %s,table: %s configure error\r\n", FREEZE_DB,
					        freeze_module_array[freeze_mod_id].module_table);
					return RE_OP_FAIL;
				}
			}

	}

	/*config value in config table*/
	result = sql_select(FREEZE_DB, FREEZE_CONFIG_TABLE,
	        dev_freeze_select_module_st, dev_freeze_config_module, NULL);
	if (result != 0)
	{
		match_record_flag = 0;
		printf("error: db: %s,freeze: %s disable or do not exist\r\n",
		        FREEZE_DB,
		        FREEZE_STATUS_TABLE);
		return RE_OP_FAIL;
	}
	if (match_record_flag)
	{
		match_record_flag = 0;

	}
	else
	{
		match_record_flag = 0;
		printf("error: db: %s,table: %s set error\r\n", FREEZE_DB,
		FREEZE_CONFIG_TABLE);
		return RE_OP_FAIL;
	}

	/*Update value in status table*/
	result = sql_update(FREEZE_DB, FREEZE_STATUS_TABLE,
	        dev_freeze_select_module_st, dev_freeze_update_module_st, NULL);
	if (result != 0)
	{
		printf("error: db: %s,freeze: %s disable or do not exist\r\n",
		        FREEZE_DB,
		        FREEZE_STATUS_TABLE);
		return RE_OP_FAIL;
	}

	return RE_SUCCESS;
}

void dev_freeze_remind_ctrl(void)
{
	static int count = 0;
	if (temp_info.remind_start_count != 0)
	{
		count++;
		printf("remind count is %d,delay is %d",count, temp_info.remind_delay);
		if ((count * 5) == (temp_info.remind_delay))
		{
			count = 0;
			if (temp_info.remind_start_count == 1)
			{
				temp_info.remind_stop_count = 1;
			}
			else if (temp_info.remind_start_count == 2)
			{
				temp_info.remind_stop_count = 2;
			}
			printf("remind count stop ,count :%d\r\n",temp_info.remind_stop_count);
		}
		temp_info.remind_start_count = 0;
	}
	else
	{
		count = 0;
		printf("reset count\r\n");
	}
}
re_error_enum dev_freeze_module_monitor(void)
{
	re_error_enum re_val = RE_SUCCESS;
	int i;
	while (1)
	{
		printf("freeze module thread\r\n");
		re_val = dev_freeze_module_init();
		if (re_val != RE_SUCCESS)
		{
			printf("error: freeze module init failed\r\n");
			continue;
		}
		for (i = 0; i < match_freeze_module_num; i++)
		{
			pthread_mutex_lock(&thread_mutex);
			re_val = dev_freeze_module_switch(i);
			if (re_val != RE_SUCCESS)
			{
				printf("error: freeze module %d: operation failed\r\n", i);
			}
			pthread_mutex_unlock(&thread_mutex);
		}
		sleep(5);
		dev_freeze_remind_ctrl();
	}

	return RE_SUCCESS;
}
