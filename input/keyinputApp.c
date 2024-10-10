#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>


int main(int argc, char *argv[])
{
	int fd, ret;
	struct input_event ev;

	if(2 != argc) {
		printf("Usage:\n"
			 "\t./keyinputApp /dev/input/eventX    @ Open Key\n"
		);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if(0 > fd) {
		printf("Error: file %s open failed!\r\n", argv[1]);
		return -1;
	}

	for ( ; ; ) {

		ret = read(fd, &ev, sizeof(struct input_event));
		if (ret) {
			switch (ev.type) {
			case EV_KEY:
				if (KEY_0 == ev.code) {
					if (ev.value)
						printf("Key0 Press\n");
					else
						printf("Key0 Release\n");
				}
				break;


			case EV_REL:
				break;
			case EV_ABS:
				break;
			case EV_MSC:
				break;
			case EV_SW:
				break;
			};
		}
		else {
			printf("Error: file %s read failed!\r\n", argv[1]);
			goto out;
		}
	}

out:
	close(fd);
	return 0;
}