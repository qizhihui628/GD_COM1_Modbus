#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include "serial.h"

static fd_set readfs;

void serial_init(void)
{
	FD_ZERO(&readfs);
}

re_error_enum serial_creat(char* dev_name_ptr, u32 baud_rate, int* fd_ptr)
{
	int fd, baud;
	struct termios newtio;
	if (dev_name_ptr == NULL || fd_ptr == NULL)
	{
		return RE_INVALID_POINTER;
	}
	switch (baud_rate)
	{
	case 9600:
		baud = B9600;
		break;
	case 115200:
		baud = B115200;
		break;
	default:
		return RE_INVALID_PARAMETER;

	}
#ifdef DEBUG
	printf("creat1 success\r\n");
#endif
	fd = open(dev_name_ptr, O_RDWR | O_NOCTTY);
#ifdef DEBUG
	printf("creat success\r\n");
#endif
	if (fd < 0)
	{
		perror(dev_name_ptr);
		return RE_OP_FAIL;
	}

	tcgetattr(fd, &newtio);
	cfsetispeed(&newtio, baud);
	cfsetospeed(&newtio, baud);

	if (tcsetattr(fd, TCSANOW, &newtio) != 0)
	{
		perror("tcsetattr error");
		return RE_OP_FAIL;
	}

	newtio.c_cflag &= ~CSIZE;
	newtio.c_cflag |= CS8;
	newtio.c_cflag &= ~CSTOPB;
	newtio.c_cflag &= ~PARENB;
	//  newtio.c_cflag &= ~INPCK;   			no need~~~
	newtio.c_cflag |= (CLOCAL | CREAD);

	newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	newtio.c_oflag &= ~OPOST;
	newtio.c_oflag &= ~(ONLCR | OCRNL);    //添加的

	newtio.c_iflag &= ~(ICRNL | INLCR);
	newtio.c_iflag &= ~(IXON | IXOFF | IXANY);    //添加的

	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 0;

	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &newtio) != 0)
	{
		perror("serial error");
		return RE_OP_FAIL;
	}

	*fd_ptr = fd;
	return RE_SUCCESS;

}

re_error_enum serial_write(int fd, u8 *buf_ptr, u8 buf_size)
{
	int i;
	if (buf_ptr == NULL)
	{
		return RE_INVALID_POINTER;
	}
#ifdef DEBUG
	printf("send  %d bytes:\n", buf_size);
	for (i = 0; i < buf_size; i++)
	{
		printf("%02x ", buf_ptr[i]);
	}
	printf("\r\n");
#endif
	if (write(fd, buf_ptr, buf_size) < 0)
	{
		printf("error: write %s failed\n", buf_ptr);
		return RE_OP_FAIL;
	}

	return RE_SUCCESS;
}

re_error_enum serial_read(int fd, u8 max_size, u8 *buf_ptr, u8* buf_size_ptr)
{
	int res = 0;
	int i;

	if (buf_ptr == NULL)
	{
		return RE_INVALID_POINTER;
	}

	res = read(fd, buf_ptr, max_size);

	if (res <= 0)
	{
		printf("error: read timeout\n");
		return RE_OP_FAIL;
	}
#ifdef DEBUG
	printf("read  %d success:\n", res);
	for (i = 0; i < res; i++)
	{
		printf("%02x ", buf_ptr[i]);
	}
	printf("\r\n");
#endif

	*buf_size_ptr = res;
	return RE_SUCCESS;
}
