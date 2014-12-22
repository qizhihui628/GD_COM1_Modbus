#include "type.h"

u16 CRC16_check(u8 *input_ptr, u8 length)
{
	u16 reg_crc = 0xffff;
	u8 temp_reg = 0x00;
	u8 i, j;
	for (i = 0; i < length; i++)
	{
		reg_crc ^= *input_ptr++;
		for (j = 0; j < 8; j++)
		{
			if (reg_crc & 0x0001)
			{
				reg_crc = reg_crc >> 1 ^ 0xA001;
			}
			else
			{
				reg_crc >>= 1;
			}
		}
	}
	temp_reg = reg_crc >> 8;
	return (reg_crc << 8 | temp_reg);

}

u16 checksum(u8 *input_ptr, u8 length)
{
	u16 check_sum = 0x0000;
	u8 i;

	for(i = 0; i < length; i++)
	{
		check_sum += input_ptr[i];
	}
	check_sum = ~(check_sum & 0x00ffff) + 1;

	return check_sum;

}

re_error_enum hex2asc(u8 hex, u8* asc1, u8* asc2)
{

	*asc1 = (hex & 0xf0) >> 4;
	if ((*asc1 <= 9) && (*asc1 >= 0))
	{
		*asc1 += 0x30;
	}
	else
	{
		*asc1 += 0x37;
	}

	*asc2 = (hex & 0x0f);
	if ((*asc2 >= 0) && (*asc2 <= 9))
	{
		*asc2 += 0x30;
	}
	else
	{
		*asc2 += 0x37;
	}
	return RE_SUCCESS;
}

u8 val2asc(u8 value)
{
	u8 asc;
	asc = (value & 0xf0) >> 4;
	if ((asc <= 9) && (asc >= 0))
	{
		asc += 0x30;
	}
	else
	{
		asc += 0x37;
	}

	return asc;
}

u8 asc2val(u8 asc)
{
	u8 val;

	if (asc >= 0x30 && asc <= 0x39)
	{
		val = asc - 0x30;
	}
	else if (asc >= 0x41 && asc <= 0x46)
	{
		val = asc - 0x37;
	}
	else //for 20?
	{
		val = 0;
	}
	return val;
}

u8 asc2hex(u8 asc1, u8 asc2)
{

	u8 result, result1;

	if (asc1 >= 0x30 && asc1 <= 0x39)
	{
		result = asc1 - 0x30;
	}
	else if (asc1 >= 0x41 && asc1 <= 0x46)
	{
		result = asc1 - 0x37;
	}
	else //for 20?
	{
		result = 0;
	}

	if (asc2 >= 0x30 && asc2 <= 0x39)
	{
		result1 = asc2 - 0x30;
	}
	else if (asc2 >= 0x41 && asc2 <= 0x46)
	{
		result1 = asc2 - 0x37;
	}
	else //for 20?
	{
		result1 = 0;
	}

	return (((result & 0x0F) << 4) + (result1 & 0x0F));
}

