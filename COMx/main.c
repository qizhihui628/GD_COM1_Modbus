#include <stdio.h>
#include "main.h"
#include "pthread.h"
#include "modbus_master.h"
#include "dev_light_mod.h"
#include "dev_XR75CX_module.h"
#include "dev_daikin_module.h"
#include "dev_power_module.h"

#define NONE_STANDARD 0
#define ST_MODEBUS 1
#define AD_IO_MODBUS 2
#define PROTOCAL_NUM 3
#define CURRENT_PROTOCAL ST_MODEBUS
#define DEV_AMS_ADDR 0x01
#define DEV_LIGHT_MODUL_ADDR 0x01

pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *dev_name_array[PROTOCAL_NUM] = { "/dev/ttyS1", "/dev/ttyS1", "/dev/ttyS1" };
//static char *dev_name_array[PROTOCAL_NUM] = {"/dev/ttymxc1", "/dev/ttymxc1", "/dev/ttymxc2"};
int main(void)
{
	re_error_enum re_val = RE_SUCCESS;
	bool loop = 1; /* loop while TRUE */
	pthread_t pid1, pid2;
	modbus_init();
	/* open_input_source opens a device, sets the port correctly, and
	 returns a file descriptor */
	// open_input_source
#if CURRENT_PROTOCAL == NONE_STANDARD
	if (nonstd_creat(dev_name_array[NONE_STANDARD], 9600) != RE_SUCCESS)
	{
		printf("error : none standard protocol init failed\n");
		//exit(1);
	}
#elif CURRENT_PROTOCAL == ST_MODEBUS
	if (modbus_creat(dev_name_array[ST_MODEBUS], 9600) != RE_SUCCESS)
	{
		printf("error: modbus protocol init failed\n");
		return 1;
	}
#else
	if (modbus_creat(dev_name_array[AD_IO_MODBUS], 9600) != RE_SUCCESS)
	{
		printf("error: modbus protocol init failed\n");
		return 1;
	}
#endif
	pthread_create(&pid1, NULL, dev_power_module_monitor, NULL);
	pthread_create(&pid2, NULL, dev_light_module_monitor, NULL);
	pthread_create(&pid2, NULL, dev_freeze_module_monitor, NULL);
	/* loop for input */
	while (loop)
	{
		/*
		re_val = dev_light_module_monitor();
		if (re_val != RE_SUCCESS)
		{
			printf("error:dev_light_module_monitor failed\n");
			return 1;
		}

		re_val = dev_freeze_module_monitor();
		if (re_val != RE_SUCCESS)
		{
			printf("error:dev_freeze_module_monitor failed\n");
			return 1;
		}

		re_val = dev_power_module_monitor();
				if (re_val != RE_SUCCESS)
				{
					printf("error:dev_power_module_monitor failed\n");
					return 1;
				}
		//break;
		 *
		 */

	}
	sleep(1);
	return 1;
}
