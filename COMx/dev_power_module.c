#include "modbus_master.h"
#include "dev_power_module.h"
#include "system.h"
#include "sql_op.h"
#include <stdio.h>

#define MODBUS_CONFIG_DB "./DataBase/ModBus_Config.db"
#define POWER_REGISTER_TABLE "Power_Register"
#define POWER_ADDR "Address"
#define POWER_STATUS "Status"
#define POWER_TABLE "Table_Name"

#define POWER_DB "./DataBase/Power.db"

#define POWER_ADDRESS "Address"
#define POWER_DAY_TABLE "Power_Day_Record"

#define POWER_NUMBER            3
#define POWER_REGISTER_ADDR     0x0384
#define POWER_VALUE_NUM         0x24
#define MAX_POWER_MODULE_NUMBER 2

static u8 match_record_flag = 0;
static u8 match_power_module_num = 0;
static u8 current_module_id = 0;
static u8 date_change_flag = 0;
static power_module_struct power_module_array[MAX_POWER_MODULE_NUMBER];

static float cur_energy_value_array[POWER_NUMBER] = {0};

static void dev_power_select_modue(void);

static void dev_power_select_modue(void)
{
	sql_select_where_equal(POWER_STATUS, "enable");
}

static void dev_power_status_update(void *value_ptr)
{
	u8 val_num;
	u8 val_buf[POWER_VALUE_NUM] = { 0 };
	char cur_val[10];
	re_error_enum re_val = RE_SUCCESS;
	float vol, cur, pwr;
	static float pre_pwr[POWER_NUMBER] = {0};
	u32 tmp;
	int i;

	re_val = modbus_read_hold_reg(POWER_REGISTER_ADDR, POWER_NUMBER, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		return;
	}
	if (val_num != POWER_VALUE_NUM)
	{
		printf("error %d: serial read line value invalid\n", re_val);
		return;
	}
	sql_add("(null,");
	for (i = 0; i < POWER_VALUE_NUM/4; i++)
	{
		tmp = ((u32)val_buf[i*4] << 24) | ((u32)val_buf[i*4+1] << 16) | ((u32)val_buf[i*4+2] << 8)| ((u32)val_buf[i*4+3]);
		if((i % 3) == 0)
		{
			if (i != 0)
			{
				sql_add(",");
			}
			vol = (float)tmp / 10;
			sprintf((char*) cur_val, "%.1f", vol);
			sql_add(cur_val);
			sql_add(",");
		}
		else if ((i % 3) == 1)
		{

			cur = (float)tmp / 10;
			sprintf((char*) cur_val, "%.1f", cur);
			sql_add(cur_val);
			sql_add(",");
		}
		else
		{
			pwr = (float)tmp / 100;
			sprintf((char*) cur_val, "%.2f", pwr);
			sql_add(cur_val);
			sql_add(",");

			cur_energy_value_array[i/3] += pre_pwr[i/3] * 10;
			sprintf((char*) cur_val, "%.2f", cur_energy_value_array[i/3]);
			sql_add(cur_val);
			pre_pwr[i/3] = pwr;
		}
	}
	sql_add(",datetime('now','localtime'))");
}

static void dev_power_day_status_update(void *value_ptr)
{
	u8 val_num;
	u8 val_buf[POWER_VALUE_NUM] = { 0 };
	char cur_val[10];
	re_error_enum re_val = RE_SUCCESS;
	int i;

	re_val = modbus_read_hold_reg(POWER_REGISTER_ADDR, POWER_NUMBER, &val_num, val_buf);

	if (re_val != RE_SUCCESS)
	{
		printf("error %d: serial read line failed\n", re_val);
		return;
	}
	if (val_num != POWER_VALUE_NUM)
	{
		printf("error %d: serial read line value invalid\n", re_val);
		return;
	}
	sql_add("(null,");

	for(i = 0; i < POWER_NUMBER; i++)
	{
		if (i != 0)
		{
			sql_add(",");
		}
		sprintf((char*) cur_val, "%.2f", cur_energy_value_array[i]);
		sql_add(cur_val);
	}

	sql_add(",date('now','-1 day'))");
}

static int enter_record_get_module_info(void * para, int n_column,
        char ** column_value, char ** column_name)
{
	int i;

	for (i = 0; i < n_column; i++)
	{
		if (strcmp(column_name[i], POWER_ADDR) == 0)
		{

			power_module_array[match_power_module_num].module_addr = atoi(
			        column_value[i]);
			match_record_flag++;
		}
		if (strcmp(column_name[i], POWER_TABLE) == 0)
		{
			printf("coumn_value is %s\r\n", column_value[i]);
			strcpy(power_module_array[match_power_module_num].module_table ,column_value[i]);
			match_record_flag++;
		}

	}
	match_power_module_num++;
	return 0;
}

static re_error_enum dev_power_module_init(void)
{
	int result;
	memset(power_module_array, 0, sizeof(power_module_array));
	match_power_module_num = 0;
	current_module_id = 0;

	result = sql_select(MODBUS_CONFIG_DB, POWER_REGISTER_TABLE,
	        dev_power_select_modue, enter_record_get_module_info, NULL);
	if (result != 0)
	{
		printf("error: db: %s,power: %s disable or do not exist\r\n",
		MODBUS_CONFIG_DB, POWER_REGISTER_TABLE);
		return RE_OP_FAIL;
	}

	if (match_record_flag == 2)
	{
		match_record_flag = 0;
	}
	else
	{
		match_record_flag = 0;
		match_power_module_num = 0;
		printf("error: db: %s,power: %s disable or do not exist\r\n",
		MODBUS_CONFIG_DB, POWER_REGISTER_TABLE);
		return RE_OP_FAIL;
	}
	return RE_SUCCESS;
}

static re_error_enum dev_power_module_switch(u8 power_mod_id)
{
	int result;
	if (power_mod_id > match_power_module_num)
	{
		printf("error: power module: %d disable or do not exist\r\n",
		        power_mod_id);
		return RE_OP_FAIL;
	}
	if (power_module_array[power_mod_id].module_addr
	        == 0|| strcmp(power_module_array[power_mod_id].module_table, "") == 0)
	{
		printf("error: power module: %d disable or do not exist\r\n",
		        power_mod_id);
		return RE_OP_FAIL;
	}
	current_module_id = power_mod_id;
	modbus_dev_switch(power_module_array[power_mod_id].module_addr);

	/*Update value in status table*/
	result = sql_insert(POWER_DB, power_module_array[power_mod_id].module_table, dev_power_status_update, NULL);
	if (result != 0)
	{
		printf("error: db: %s,power: %s disable or do not exist\r\n", POWER_DB,
				power_module_array[power_mod_id].module_table);
		return RE_OP_FAIL;
	}

	if (date_change_flag)
	{
		/*Update day value in status table*/
		result = sql_insert(POWER_DB, POWER_DAY_TABLE, dev_power_day_status_update, NULL);
		if (result != 0)
		{
			printf("error: db: %s,power: %s disable or do not exist\r\n", POWER_DB,
					power_module_array[power_mod_id].module_table);
			return RE_OP_FAIL;
		}
		result = sql_delete_tbl(POWER_DB, power_module_array[power_mod_id].module_table);
		if (result != 0)
		{
			printf("error: db: %s,power: %s disable or do not exist\r\n", POWER_DB,
					power_module_array[power_mod_id].module_table);
			return RE_OP_FAIL;
		}
	}
	return RE_SUCCESS;

}

re_error_enum dev_power_module_monitor(void)
{
	re_error_enum re_val = RE_SUCCESS;
	int i;
	int other, day, pre_day;
	get_current_time(&other, &other, &other, &day, &other, &other, &other);
	pre_day = day;
	while (1)
	{
		printf("power module thread\r\n");
		//sleep(10);
#if 1
		get_current_time(&other, &other, &other, &day, &other, &other, &other);
		printf("get_current_time->done\r\n");		//jj
		if (pre_day != day)
		{
			date_change_flag = 1;
		}
		re_val = dev_power_module_init();
		printf("dev_power_module_init->done\r\n");		//jj
		if (re_val != RE_SUCCESS)
		{
			printf("error: power module init failed\r\n");
			continue;
		}
		for (i = 0; i < match_power_module_num; i++)
		{
			re_val = dev_power_module_switch(i);
			if (re_val != RE_SUCCESS)
			{
				printf("error: power module %d: operation failed\r\n", i);
				break;
			}
		}

		sleep(4);
		pre_day = day;
#endif
	}

	return RE_SUCCESS;
}

