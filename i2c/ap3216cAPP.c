#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>


#define DEVICE_NAME "/dev/ap3216c"


int main(int argc,char** argv)
{
	int fd;
	unsigned short databuf[3];
	unsigned short ir,als,ps;
	int ret = 0;

	fd = open(DEVICE_NAME,O_RDWR);
	if (fd < 0)
	{
		printf("can't open file %s\n",DEVICE_NAME);
		return -1;
	}

	while(1)
	{
		ret = read(fd, databuf, sizeof(databuf));
		if (ret == 0)
		{
			ir = databuf[0];
			als = databuf[1];
			ps = databuf[2];
			printf("ir=%d, als=%d, ps=%d\r\n",ir,als,ps);
		}
		//usleep(20000);
		sleep(1);
	}

	return 0;
}
