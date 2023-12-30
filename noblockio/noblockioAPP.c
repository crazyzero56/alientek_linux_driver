#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	fd_set readfds;
	int key_val;
	int fd;
	int ret;

	if (argc != 2) {
		printf("usage: \n"
			   "\t./key/App /dev/key\n");
		return -1;
	}

	fd = open(argv[1], O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		printf("ERROR: %s file open faild\n", argv[1]);
		return -1;
	}
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	for (;;) {
		ret = select(fd + 1, &readfds, NULL, NULL, NULL);
		switch (ret) {
		case 0:
			/* code */
			break;
		case -1:
			/* code */
			break;
		default:
			if (FD_ISSET(fd, &readfds)) {
				read(fd, &key_val, sizeof(int));
				if (0 == key_val) {
					printf("key press\n");
				} else if (1 == key_val) {
					printf("key release\n");
				}
			}
			break;
		}
	}
	close(fd);
	return 0;
}