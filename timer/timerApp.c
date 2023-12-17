#include "fcntl.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "unistd.h"
#include <sys/ioctl.h>

#define CLOSE_CMD (_IO(0XEF, 0x1))
#define OPEN_CMD (_IO(0XEF, 0x2))
#define SETPERIOD_CMD (_IO(0XEF, 0x3))

int main(int argc, int **argv)
{
	int fd;
	int ret;
	char *filename;
	unsigned int cmd;
	unsigned int arg;
	unsigned char str[100];

	if (argc != 2) {
		printf("error usage!\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("open %s fail!\n", filename);
		return -1;
	}
	while (1) {
		printf("Input CMD:");
		ret = scanf("%d", &cmd);
		if (ret != 1) {
			fgets(str, sizeof(str), stdin);
		}
		if (4 == cmd) /* 退出APP	 */
			goto out;
		if (cmd == 1) /* 关闭LED灯 */
			cmd = CLOSE_CMD;
		else if (cmd == 2) /* 打开LED灯 */
			cmd = OPEN_CMD;
		else if (cmd == 3) {
			cmd = SETPERIOD_CMD; /* 设置周期值 */
			printf("Input Timer Period:");
			ret = scanf("%d", &arg);
			if (ret != 1) {						/* 参数输入错误 */
				fgets(str, sizeof(str), stdin); /* 防止卡死 */
			}
		}
		ioctl(fd, cmd, arg); /* 控制定时器的打开和关闭 */
	}

out:
	close(fd);
	return 0;
}