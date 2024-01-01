#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int fd;

static void sigio_signal_function(int signum)
{
	unsigned int key_val = 0;
	read(fd, &key_val, sizeof(unsigned int));
	if (key_val == 0)
		printf("key press \n");
	else if (key_val == 1)
		printf("key release \n");
}

int main(int argc, char **argv)
{
	int flag = 0;
	unsigned int key_val = 0;

	if (argc != 2) {
		printf("Usage :\n"
			   "\t ./asyncKeyApp /dev/key\n");
		return -1;
	}
	fd = open(argv[1], O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		printf("error : %s file open failed\n", argv[1]);
		return -1;
	}

	signal(SIGIO, sigio_signal_function);
	fcntl(fd, F_SETOWN, getpid());
	flag = fcntl(fd, F_GETFD);
	fcntl(fd, F_SETFL, flag | FASYNC);

	for (;;) {
		sleep(2);
	}

	close(fd);
	return 0;
}
