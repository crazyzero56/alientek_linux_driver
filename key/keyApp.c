/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名              : keyApp.c
作者                : 正点原子Linux团队
版本                : V1.0
描述                : Linux中断驱动实验
其他                : 无
使用方法            : ./keyirqApp /dev/key
论坛                : www.openedv.com
日志                : 初版V1.0 2019/1/30 正点原子Linux团队创建
***************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define KEY0VALUE 0xF0
#define INVAKEY 0x00

/*
 * @description		: main主程序
 * @param – argc		: argv数组元素个数
 * @param – argv		: 具体参数
 * @return			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, ret;
	int key_val;

	if (2 != argc) {
		printf("Usage:\n"
			   "\t./keyApp /dev/key\n");
		return -1;
	}

	/* 打开设备 */
	fd = open(argv[1], O_RDONLY);
	if (0 > fd) {
		printf("ERROR: %s file open failed!\n", argv[1]);
		return -1;
	}

	/* 循环读取按键数据 */
	for (;;) {

		read(fd, &key_val, sizeof(int));
		if (key_val == KEY0VALUE) {
			printf("key0 press, value 0x%x\n", key_val);
		}
	}

	/* 关闭设备 */
	ret = close(fd);
	if (ret < 0) {
		printf("file close failed\n", argv[1]);
		return -1;
	}

	return 0;
}
