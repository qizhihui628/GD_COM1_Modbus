#include "serial.h"
#include "crc_check.h"
#include "nonstd_master.h"

#define NONSTD_SOI         0x7E
#define NONSTD_VER         0x20
#define NONSTD_EOI         0x0D

#define NONSTD_CID1_AIR    0x60

#define NONSTD_CID2_PRIVATE             0xE0

#define NONSTD_CID2_NORMAL_RTN          0x00

#define NONSTD_PRIVATE_FUNC_CTRL        0x600
#define NONSTD_PRIVATE_FUNC_CTRL_MODE   0x620
#define NONSTD_PRIVATE_FUNC_SET_TEMP    0x630
#define NONSTD_PRIVATE_FUNC_GET_STATUS    0x400

#define NONSTD_MAX_BUF_SIZE 100
#define NONSTD_MAX_DATA_SIZE 100
#define CRC_LEN 2
#define HEAD_LEN 1
#define END_LEN 1
#define HEAD_INFO_LEN 6
#define NONSTD_INVALID_DEV 0xff
#define NONSTD_PKG_LEN(data_len) ((data_len) + CRC_LEN + HEAD_LEN + HEAD_INFO_LEN + END_LEN)

#define NONSTD_FUNC_READ_LINE 0x01
#define NONSTD_FUNC_READ_HOLD_REG 0x03
#define NONSTD_FUNC_WRITE_SINGLE_LINE 0x05
#define NONSTD_FUNC_WRITE_MUL_LINE 0x0f
#define NONSTD_FUNC_WRITE_MUL_REGISTER 0x10
typedef struct
{
	u8 soi;
	u8 ver;
	u8 addr;
	u8 cid1;
	u8 cid2;
	u16 lenid;//asccii number
	u8 *data_ptr;//ascii byte
	u16 crc_val;
	u8 eoi;
} nonstd_pkg_struct;

static int nonstd_fd = -1;

static u8 cur_nonstd_dev = NONSTD_INVALID_DEV;
static u8 cur_nonstd_air_no = NONSTD_INVALID_DEV;
static u16 nonstd_lenth_encode(u16 length)
{
	u16 temp_sum, temp_int;
	temp_sum = 0;

	temp_int = (length >> 8) & 0x0F;
	temp_sum = temp_int;
	temp_int = (length >> 4) & 0x0F;
	temp_sum += temp_int;
	temp_sum += length & 0x0F;

	temp_int = temp_sum % 16;
	temp_int = ~temp_int + 1;

	temp_sum = (temp_int << 12) & 0xF000;

	temp_sum += length & 0xFFF;

	return temp_sum;
}

static re_error_enum nonstd_lenth_decode(u16 len_info, u16* len_ptr)
{
	if (nonstd_lenth_encode(len_info & 0x0fff) == len_info)
	{
		*len_ptr = len_info & 0x0fff;
		return RE_SUCCESS;
	}
	else
	{
		printf("error: invalid length info: %d\r\n", len_info);
		return RE_OP_FAIL;
	}

}

static void nonstd_pkg_decode(nonstd_pkg_struct* pkg_ptr, u8* data_buf, u8* data_len_ptr)
{
	u8 i = 0;
	u16 len_info = 0;
	data_buf[0] = pkg_ptr->soi = NONSTD_SOI;
	pkg_ptr->ver = NONSTD_VER;
	pkg_ptr->cid1 = NONSTD_CID1_AIR;
	pkg_ptr->cid2 = NONSTD_CID2_PRIVATE;
	hex2asc(pkg_ptr->ver, &data_buf[1], &data_buf[2]);
	pkg_ptr->addr = cur_nonstd_dev;
	hex2asc(pkg_ptr->addr, &data_buf[3], &data_buf[4]);
	hex2asc(pkg_ptr->cid1, &data_buf[5], &data_buf[6]);
	hex2asc(pkg_ptr->cid2, &data_buf[7], &data_buf[8]);
	len_info = nonstd_lenth_encode(pkg_ptr->lenid);
	hex2asc(H_VAL16(len_info), &data_buf[9], &data_buf[10]);
	hex2asc(L_VAL16(len_info), &data_buf[11], &data_buf[12]);

	for (i = 0; i < pkg_ptr->lenid; i++)
	{
		data_buf[13 + i] = val2asc(pkg_ptr->data_ptr[i]);

	}
	pkg_ptr->crc_val = checksum(&data_buf[1], (12+i*2));
	hex2asc(H_VAL16(pkg_ptr->crc_val), &data_buf[13+i*2], &data_buf[14+i*2]);
	hex2asc(H_VAL16(pkg_ptr->crc_val), &data_buf[15+i*2], &data_buf[16+i*2]);
	data_buf[17+i*2] = pkg_ptr->eoi = NONSTD_EOI;

	*data_len_ptr = 18+i*2;
}

static re_error_enum nonstd_pkg_encode(u8* data_buf, nonstd_pkg_struct* pkg_ptr, u16 len)
{
	u16 i = 0;
	u16 len_info = 0;
	pkg_ptr->crc_val = asc2hex(data_buf[len - END_LEN - 2], data_buf[len - END_LEN - 1]);
	pkg_ptr->crc_val = ((u16)asc2hex(data_buf[len - END_LEN - 4], data_buf[len - END_LEN - 3]) << 8) | pkg_ptr->crc_val;
	if ( pkg_ptr->crc_val != checksum(&data_buf[1], (len - HEAD_LEN - END_LEN - CRC_LEN*2)))
	{
		printf("error:receive crc error\r\n ");
		return RE_OP_FAIL;
	}
	pkg_ptr->soi = data_buf[0];
	pkg_ptr->eoi = data_buf[len - END_LEN];
	pkg_ptr->ver = asc2hex(data_buf[1], data_buf[2]);
	pkg_ptr->addr = asc2hex(data_buf[3], data_buf[4]);

	if (pkg_ptr->soi != NONSTD_SOI || pkg_ptr->eoi != NONSTD_EOI || pkg_ptr->ver != NONSTD_VER || pkg_ptr->addr != cur_nonstd_dev)
	{
			printf("error:receive invalid package\r\n");
			return RE_OP_FAIL;
	}

	pkg_ptr->cid1 = asc2hex(data_buf[5], data_buf[6]);

	pkg_ptr->cid2 = asc2hex(data_buf[7], data_buf[8]);
	if (pkg_ptr->cid2 != NONSTD_CID2_NORMAL_RTN)
	{
		printf("error:receive invalid package cid2: %d\r\n", pkg_ptr->cid2);
		return RE_OP_FAIL;
	}
	len_info = (u16)asc2hex(data_buf[9], data_buf[10]);
	len_info = len_info |asc2hex(data_buf[11], data_buf[12]);
	if (nonstd_lenth_decode(len_info, &pkg_ptr->lenid) != RE_SUCCESS)
	{
		printf("error:receive invalid data length: %d\r\n", pkg_ptr->lenid);
		return RE_OP_FAIL;
	}
	if (pkg_ptr->lenid != (len - END_LEN - (HEAD_INFO_LEN + CRC_LEN)*2 - HEAD_LEN))
	{
		printf("error:receive invalid package length : %d\r\n", len);
		return RE_OP_FAIL;
	}

	for (i = 0; i < pkg_ptr->lenid; i++)
	{
		pkg_ptr->data_ptr[i] = asc2val(data_buf[13 + i]);

	}
	return RE_SUCCESS;
}

void nonstd_init(void)
{
	serial_init();
}

re_error_enum nonstd_creat(char* dev_name_ptr, u32 baud_rate)
{
	re_error_enum ret_val = RE_SUCCESS;
	int fd = -1;
	ret_val = serial_creat(dev_name_ptr, baud_rate, &fd);
	if (ret_val != RE_SUCCESS || fd < 0)
	{
		return RE_OP_FAIL;
	}
	nonstd_fd = fd;
	return RE_SUCCESS;
}

static re_error_enum nonstd_send(nonstd_pkg_struct *pkg_ptr)
{
	re_error_enum ret_val = RE_SUCCESS;
	u8 len = 0;
	u8 nonstd_send_buf[NONSTD_MAX_BUF_SIZE] =
	{ 0 };

	if (nonstd_fd < 0)
	{
		printf("error:creat nonstd first\r\n");
		return RE_OP_FAIL;
	}
	if (cur_nonstd_dev == NONSTD_INVALID_DEV)
	{
		printf("error:specify nonstd dev first\r\n");
		return RE_OP_FAIL;
	}

	nonstd_pkg_decode(pkg_ptr, nonstd_send_buf, &len);

#ifdef DEBUG
	printf("crc : %x, len:%d\r\n", pkg_ptr->crc_val, len);
#endif

	usleep(10000);
	ret_val = serial_write(nonstd_fd, nonstd_send_buf, len);
	return ret_val;
}

static re_error_enum nonstd_receive(nonstd_pkg_struct *pkg_ptr)
{
	re_error_enum ret_val = RE_SUCCESS;
	u8 len = 0;
	u8 nonstd_receive_buf[NONSTD_MAX_BUF_SIZE] =
	{ 0 };

	if (nonstd_fd < 0)
	{
		printf("error:creat nonstd first\r\n");
		return RE_OP_FAIL;
	}
	if (cur_nonstd_dev == NONSTD_INVALID_DEV)
	{
		printf("error:specify nonstd dev first\r\n");
		return RE_OP_FAIL;
	}
	ret_val = serial_read(nonstd_fd, NONSTD_MAX_BUF_SIZE, nonstd_receive_buf,
	        &len);
	if (ret_val != RE_SUCCESS)
	{
		return ret_val;
	}
	if (nonstd_pkg_encode(nonstd_receive_buf, pkg_ptr, len) != RE_SUCCESS)
	{
		printf("error:receive package failed\r\n");
		return RE_OP_FAIL;
	}

	return RE_SUCCESS;

}

void nonstd_dev_switch(u8 addr, u8 air_no)
{
	cur_nonstd_dev = addr;
	cur_nonstd_air_no = air_no;
}

re_error_enum nonstd_on_ff(u8 value)
{
	re_error_enum ret_val = RE_SUCCESS;
	nonstd_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[NONSTD_MAX_BUF_SIZE] =
	{ 0 };
	data_buf[0] = ((u16)NONSTD_PRIVATE_FUNC_CTRL >> 8) & 0x0f;
	data_buf[1] = ((u16)NONSTD_PRIVATE_FUNC_CTRL >> 4) & 0x0f;
	data_buf[2] = ((u16)NONSTD_PRIVATE_FUNC_CTRL) & 0x0f;
	data_buf[3] = cur_nonstd_air_no;
	data_buf[4] = value;
	pkg_ptr.lenid = 5;
	pkg_ptr.data_ptr = data_buf;

	ret_val = nonstd_send(&pkg_ptr);

	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = nonstd_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}
	if (pkg_ptr.cid1 != NONSTD_CID1_AIR)
	{
		printf("error %d: receive a unmached package,cid1: %d \r\n ", ret_val, pkg_ptr.cid1);
		return ret_val;
	}
	return ret_val;
}

re_error_enum nonstd_ctrl_mode(u8 value)
{
	re_error_enum ret_val = RE_SUCCESS;
	nonstd_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[NONSTD_MAX_BUF_SIZE] =
	{ 0 };
	data_buf[0] = ((u16)NONSTD_PRIVATE_FUNC_CTRL_MODE >> 8) & 0x0f;
	data_buf[1] = ((u16)NONSTD_PRIVATE_FUNC_CTRL_MODE >> 4) & 0x0f;
	data_buf[2] = ((u16)NONSTD_PRIVATE_FUNC_CTRL_MODE) & 0x0f;
	data_buf[3] = cur_nonstd_air_no;
	data_buf[4] = value;
	pkg_ptr.lenid = 5;
	pkg_ptr.data_ptr = data_buf;

	ret_val = nonstd_send(&pkg_ptr);

	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = nonstd_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}
	if (pkg_ptr.cid1 != NONSTD_CID1_AIR)
	{
		printf("error %d: receive a unmached package,cid1: %d \r\n ", ret_val, pkg_ptr.cid1);
		return ret_val;
	}
	return ret_val;
}

re_error_enum nonstd_set_temp(u16 hot_val, u16 cool_val)
{
	re_error_enum ret_val = RE_SUCCESS;
	nonstd_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[NONSTD_MAX_BUF_SIZE] =
	{ 0 };
	data_buf[0] = ((u16)NONSTD_PRIVATE_FUNC_SET_TEMP >> 8) & 0x0f;
	data_buf[1] = ((u16)NONSTD_PRIVATE_FUNC_SET_TEMP >> 4) & 0x0f;
	data_buf[2] = ((u16)NONSTD_PRIVATE_FUNC_SET_TEMP) & 0x0f;
	data_buf[3] = cur_nonstd_air_no;
	data_buf[4] = (((u16)cool_val/100) >> 8) & 0x0f;
	data_buf[5] = (((u16)cool_val/10) >> 4) & 0x0f;
	data_buf[6] = (((u16)cool_val%10)) & 0x0f;
	data_buf[7] = (((u16)hot_val/100) >> 8) & 0x0f;
	data_buf[8] = (((u16)hot_val/10) >> 4) & 0x0f;
	data_buf[9] = (((u16)hot_val%10)) & 0x0f;
	pkg_ptr.lenid = 10;
	pkg_ptr.data_ptr = data_buf;

	ret_val = nonstd_send(&pkg_ptr);

	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);
	ret_val = nonstd_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}
	if (pkg_ptr.cid1 != NONSTD_CID1_AIR)
	{
		printf("error %d: receive a unmached package,cid1: %d \r\n ", ret_val, pkg_ptr.cid1);
		return ret_val;
	}
	return ret_val;
}

re_error_enum nonstd_get_status(u8* run_status, u8* run_mode, u16* run_temp)
{
	re_error_enum ret_val = RE_SUCCESS;
	nonstd_pkg_struct pkg_ptr =
	{ 0 };
	u8 data_buf[NONSTD_MAX_BUF_SIZE] =
	{ 0 };
	data_buf[0] = ((u16)NONSTD_PRIVATE_FUNC_GET_STATUS >> 8) & 0x0f;
	data_buf[1] = ((u16)NONSTD_PRIVATE_FUNC_GET_STATUS >> 4) & 0x0f;
	data_buf[2] = ((u16)NONSTD_PRIVATE_FUNC_GET_STATUS) & 0x0f;
	data_buf[3] = cur_nonstd_air_no;
	data_buf[4] = 0;
	data_buf[5] = 0;

	pkg_ptr.lenid = 6;
	pkg_ptr.data_ptr = data_buf;

	ret_val = nonstd_send(&pkg_ptr);

	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line faild\r\n ", ret_val);
		return ret_val;
	}
	usleep(100000);

	ret_val = nonstd_receive(&pkg_ptr);
	if (ret_val != RE_SUCCESS)
	{
		printf("error %d: write line respond faild\r\n ", ret_val);
		return ret_val;
	}
	if (pkg_ptr.cid1 != NONSTD_CID1_AIR)
	{
		printf("error %d: receive a unmached package,cid1: %d \r\n ", ret_val, pkg_ptr.cid1);
		return ret_val;
	}
	*run_status = pkg_ptr.data_ptr[4] & 0x01;
	*run_mode = pkg_ptr.data_ptr[6] & 0x03;
	*run_temp = (u16)pkg_ptr.data_ptr[21] * 100;
	*run_temp += (u16)pkg_ptr.data_ptr[22] * 10;
	*run_temp += (u16)pkg_ptr.data_ptr[23];

	return ret_val;
}
