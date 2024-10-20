KERNELDIR := /home/davidhsieh/work/C_workspace/alientek_linux/
TOOLCHAIN := ~/work/C_workspace/toolchain/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
CURRENT_PATH := $(shell pwd)
NFS_PATH := /home/davidhsieh/work/linux/nfs/rootfs/lib/modules

APP_NAME := icm20608App.c
APP_TARGET = $(APP_NAME:.c=)
APP_CFLAG = -march=armv7-a -mfpu=neon -mfloat-abi=hard

obj-m := icm20608.o
target = $(patsubst %.o,%.ko, $(obj-m))
targetdis = $(patsubst %.ko,%.dis, $(target))

all: kernel_modules app install

kernel_modules:
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
	@$(TOOLCHAIN)objdump -dS $(target) > $(targetdis)

app:
	$(TOOLCHAIN)gcc $(APP_CFLAG) $(APP_NAME) -o $(APP_TARGET)

install:
	sudo cp $(APP_TARGET) $(NFS_PATH)
	sudo cp $(target) $(NFS_PATH)

clean:
	@echo clean
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	@-rm $(APP_TARGET)
	@-rm $(targetdis)
	@ls -l
