KERNELDIR := /home/davidhsieh/work/C_workspace/alientek_linux/
TOOLCHAIN := ~/work/C_workspace/toolchain/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
CURRENT_PATH := $(shell pwd)
NFS_PATH := /home/davidhsieh/work/linux/nfs/rootfs/lib/modules

#APP_NAME := blockApp.c
APP_TARGET = $(APP_NAME:.c=)

obj-m := leddevice.o
obj-m += leddriver.o
target = $(patsubst %.o,%.ko, $(obj-m))
targetdis = $(patsubst %.ko,%.dis, $(target))

all: kernel_modules install #app
#all: kernel_modules
kernel_modules:
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
	@echo "$(target)"
	@$(TOOLCHAIN)objdump -dS leddriver.ko > leddriver.dis
	@$(TOOLCHAIN)objdump -dS leddevice.ko > leddevice.dis


#app:
#	$(TOOLCHAIN)gcc $(APP_NAME) -o $(APP_TARGET)

install:
	#sudo cp $(APP_TARGET) $(NFS_PATH)
	sudo cp $(target) $(NFS_PATH)

clean:
	@echo clean
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	@-rm $(APP_TARGET)
	@-rm *.dis
	@ls -l
