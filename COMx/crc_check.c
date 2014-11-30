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
