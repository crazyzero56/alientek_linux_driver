#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LEDOFF 0
#define LEDON 1

int main(int argc, char **argv)
{
	int fd, retvalue;
	char *filename;
	unsigned char cnt = 0;
	unsigned char databuf[1];

	if (argc != 3) {
		printf("error usage\n");
		return -1;
	}
	filename = argv[1];

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("file %s open error\n", argv[1]);
		return -1;
	}
	databuf[0] = atoi(argv[2]);

	retvalue = write(fd, databuf, sizeof(databuf));
	if (retvalue < 0) {
		printf("led control fail\n");
		return -1;
	}

	while (1) {
		sleep(5);
		cnt++;
		printf("App running times %d \r\n", cnt);
		if (cnt >= 5) {
			break;
		}
	}

	printf("app run finish\n");
	retvalue = close(fd);
	if (retvalue < 0) {
		printf("file %s close failed!\n", argv[1]);
		return -1;
	}

	return 0;
}