#include "serial.h"
#include "crc_check.h"
#include "modbus_master.h"

#define MODBUS_MAX_BUF_SIZE 50
#define MODBUS_MAX_DATA_SIZE 50
#define CRC_LEN 2
#define HEAD_LEN 2
#define MODBUS_INVALID_DEV 0xff
#define MODBUS_PKG_LEN(data_len) ((data_len) + CRC_LEN + HEAD_LEN)

#define MODBUS_FUNC_READ_LINE 0x01
#define MODBUS_FUNC_READ_HOLD_REG 0x03
#define MODBUS_FUNC_WRITE_SINGLE_LINE 0x05
#define MODBUS_FUNC_WRITE_MUL_LINE 0x0f
#define MODBUS_FUNC_WRITE_MUL_REGISTER 0x10
typedef struct
{
	u8 addr;
	u8 func;
	u8 *data_ptr;
	u16 crc_val;
} modbus_pkg_struct;

static int modbus_fd = -1;

static u8 cur_modbus_dev = MODBUS_INVALID_DEV;
void modbus_init(void)
{
	serial_init();
}

re_error_enum modbus_creat(char* dev_name_ptr, u32 baud_rate)
{
	re_error_enum ret_val = RE_SUCCESS;
	int fd = -1;
	ret_val = serial_creat(dev_name_ptr, baud_rate, &fd);
	if (ret_val != RE_SUCCESS || fd < 0)
	{
		return RE_OP_FAIL;
	}
	modbus_fd = fd;
	return RE_SUCCESS;
}

re_error_enum modbus_send(modbus_pkg_struct *pkg_ptr, u8 data_len)
{
	re_error_enum ret_val = RE_SUCCESS;
	u8 i;
	u8 len = 0;
	u8 modbus_send_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };

	if (modbus_fd < 0)
	{
		printf("error:creat modbus first\r\n");
		return RE_OP_FAIL;
	}
	if (cur_modbus_dev == MODBUS_INVALID_DEV)
	{
		printf("error:specify modbus dev first\r\n");
		return RE_OP_FAIL;
	}
	modbus_send_buf[0] = pkg_ptr->addr = cur_modbus_dev;
	modbus_send_buf[1] = pkg_ptr->func;
	for (i = 0; i < data_len; i++)
	{
		modbus_send_buf[2 + i] = *pkg_ptr->data_ptr++;

	}
	len = data_len + HEAD_LEN;
	pkg_ptr->crc_val = CRC16_check(modbus_send_buf, len);
#ifdef DEBUG
	printf("crc : %x, len:%d\r\n", pkg_ptr->crc_val, len);
#endif
	modbus_send_buf[len] = H_VAL16(pkg_ptr->crc_val);
	modbus_send_buf[len + 1] = L_VAL16(pkg_ptr->crc_val);
	usleep(10000);
	ret_val = serial_write(modbus_fd, modbus_send_buf,
	        MODBUS_PKG_LEN(data_len));
	return ret_val;
}

re_error_enum modbus_receive(modbus_pkg_struct *pkg_ptr)
{
	re_error_enum ret_val = RE_SUCCESS;
	u8 len = 0, i;
	u8 modbus_receive_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };

	if (modbus_fd < 0)
	{
		printf("error:creat modbus first\r\n");
		return RE_OP_FAIL;
	}
	if (cur_modbus_dev == MODBUS_INVALID_DEV)
	{
		printf("error:specify modbus dev first\r\n");
		return RE_OP_FAIL;
	}
	ret_val = serial_read(modbus_fd, MODBUS_MAX_BUF_SIZE, modbus_receive_buf,
	        &len);
	if (ret_val != RE_SUCCESS)
	{
		return ret_val;
	}
	pkg_ptr->crc_val = CRC16_check(modbus_receive_buf, (len - CRC_LEN));
	if (L_VAL16(pkg_ptr->crc_val) == modbus_receive_buf[len - 1]
	        && H_VAL16(pkg_ptr->crc_val) == modbus_receive_buf[len - CRC_LEN])
	{
		pkg_ptr->addr = modbus_receive_buf[0];
		if (pkg_ptr->addr != cur_modbus_dev)
		{
			printf("error:receive invalid package\r\n");
			return RE_OP_FAIL;
		}
		pkg_ptr->func = modbus_receive_buf[1];
		for (i = 0; i < (len - CRC_LEN - HEAD_LEN); i++)
		{
			pkg_ptr->data_ptr[i] = modbus_receive_buf[i + HEAD_LEN];
		}
	}
	else
	{
		printf("error:receive crc error\r\n ");
		return RE_OP_FAIL;
	}

	return RE_SUCCESS;

}

void modbus_dev_switch(u8 addr)
{
	cur_modbus_dev = addr;
}

re_error_enum modbus_write_mul_line(u8 start_line, u8 line_num, u8 val)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };
	pkg_ptr.func = MODBUS_FUNC_WRITE_MUL_LINE;
	data_buf[0] = H_VAL4(start_line);
	data_buf[1] = L_VAL4(start_line);
	data_buf[2] = H_VAL4(line_num);
	data_buf[3] = L_VAL4(line_num);
	data_buf[4] = 0x01;
	data_buf[5] = val;
	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 6);

	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}

	return ret_val;
}

re_error_enum modbus_read_line(u8 start_line, u8 line_num, u8* val_num_ptr,
        u8 *val_ptr)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };
	u8 i;
	pkg_ptr.func = MODBUS_FUNC_READ_LINE;
	data_buf[0] = H_VAL4(start_line);
	data_buf[1] = L_VAL4(start_line);
	data_buf[2] = H_VAL4(line_num);
	data_buf[3] = L_VAL4(line_num);
	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 4);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: read line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: read line respond faild\r\n ", ret_val);
		return ret_val;
	}
	*val_num_ptr = pkg_ptr.data_ptr[0];
	for (i = 0; i < *val_num_ptr; i++)
	{
		val_ptr[i] = pkg_ptr.data_ptr[1 + i];
	}
	return ret_val;
}

re_error_enum modbus_write_line(u8 line_id, u8 val)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };

	pkg_ptr.func = MODBUS_FUNC_WRITE_SINGLE_LINE;
	data_buf[0] = H_VAL4(line_id);
	data_buf[1] = L_VAL4(line_id);
	if (val == 1)
	{
		data_buf[2] = 0xff;
		data_buf[3] = 0;
	}
	else if (val == 0)
	{
		data_buf[2] = 0;
		data_buf[3] = 0;
	}
	else
	{
		return RE_INVALID_PARAMETER;
	}

	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 4);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}

	return ret_val;
}
re_error_enum modbus_write_reg(u16 reg_id, u16 val)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };

	pkg_ptr.func = MODBUS_FUNC_WRITE_SINGLE_LINE;
	data_buf[0] = H_VAL16(reg_id);
	data_buf[1] = L_VAL4(reg_id);
	if (val == 1)
	{
		data_buf[2] = 0xff;
		data_buf[3] = 0;
	}
	else if (val == 0)
	{
		data_buf[2] = 0;
		data_buf[3] = 0;
	}
	else
	{
		return RE_INVALID_PARAMETER;
	}

	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 4);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}

	return ret_val;
}
re_error_enum modbus_write_mul_reg(u16 start_reg, u16 reg_num, s16 val)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };
	pkg_ptr.func = MODBUS_FUNC_WRITE_MUL_REGISTER;
	data_buf[0] = H_VAL16(start_reg);
	data_buf[1] = L_VAL16(start_reg);
	data_buf[2] = H_VAL16(reg_num);
	data_buf[3] = L_VAL16(reg_num);
	data_buf[4] = 0x02;
	data_buf[5] = (val >> 8) & 0x00ff;
	data_buf[6] = (val & 0x00ff);
	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 7);

	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}

	return ret_val;
}

re_error_enum modbus_read_binary(u16 start_reg, u16 reg_num, u8* val_num_ptr,
        u8 *val_ptr)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };
	u8 i;
	pkg_ptr.func = MODBUS_FUNC_READ_LINE;
	data_buf[0] = H_VAL16(start_reg);
	data_buf[1] = L_VAL16(start_reg);
	data_buf[2] = H_VAL16(reg_num);
	data_buf[3] = L_VAL16(reg_num);
	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 4);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: read reg: %d faild\r\n ", ret_val, start_reg);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: read reg: respond faild\r\n ", ret_val, start_reg);
		return ret_val;
	}
	*val_num_ptr = pkg_ptr.data_ptr[0];
	for (i = 0; i < *val_num_ptr; i++)
	{
		val_ptr[i] = pkg_ptr.data_ptr[1 + i];
	}
	return ret_val;
}

re_error_enum modbus_read_hold_reg(u16 start_reg, u16 reg_num, u8* val_num_ptr,
        u8 *val_ptr)
{
	re_error_enum ret_val = RE_SUCCESS;
	modbus_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[MODBUS_MAX_BUF_SIZE] =
	{ 0 };
	u8 i;
	pkg_ptr.func = MODBUS_FUNC_READ_HOLD_REG;
	data_buf[0] = H_VAL16(start_reg);
	data_buf[1] = L_VAL16(start_reg);
	data_buf[2] = H_VAL16(reg_num);
	data_buf[3] = L_VAL16(reg_num);
	pkg_ptr.data_ptr = data_buf;
	ret_val = modbus_send(&pkg_ptr, 4);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: read reg: %d faild\r\n ", ret_val, start_reg);
		return ret_val;
	}
	usleep(100000);
	ret_val = modbus_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: read reg: %d faild\r\n ", ret_val, start_reg);
		return ret_val;
	}
	*val_num_ptr = pkg_ptr.data_ptr[0];
	for (i = 0; i < *val_num_ptr; i++)
	{
		val_ptr[i] = pkg_ptr.data_ptr[1 + i];
	}
	return ret_val;
}
