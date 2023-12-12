KERNELDIR := /home/davidhsieh/work/C_workspace/alientek_linux/
TOOLCHAIN := ~/work/C_workspace/toolchain/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
CURRENT_PATH := $(shell pwd)
NFS_PATH := /home/davidhsieh/work/linux/nfs/rootfs/lib/modules
CC := $(TOOLCHAIN)gcc
DUMP := $(TOOLCHAIN)objdump
STRIP := $(TOOLCHAIN)strip

APP_NAME := mutexAPP.c
APP_TARGET = $(APP_NAME:.c=)

obj-m := mutex.o
target = $(patsubst %.o,%.ko, $(obj-m))
targetdis = $(patsubst %.ko,%.dis, $(target))
targetstrip = $(patsubst %.ko,%.strip, $(target))

all: kernel_modules app  install #strip

kernel_modules:
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
	@$(DUMP) -dS $(target) > $(targetdis)

app:
	$(CC) $(APP_NAME) -o $(APP_TARGET)

#strip:
#	@cp $(target) $(targetstrip)
#	#@$(STRIP) --strip-debug $(targetstrip)
#	@$(STRIP) $(targetstrip)
# linux kernel modules must have symbol, so do not strip
# error msg :
#[2023-10-21 23:51:56] [  147.703902] spinlock: module has no symbols (stripped?)
#[2023-10-21 23:51:56] [  147.708981] spinlock: module has no symbols (stripped?)
#[2023-10-21 23:51:56] modprobe: can't load module spinlock.ko (spinlock.ko): invalid module format

install:
	sudo cp $(APP_TARGET) $(NFS_PATH)
	sudo cp $(target) $(NFS_PATH)
	@#@sudo cp $(targetstrip) $(NFS_PATH)/$(target)
	ls -lF $(NFS_PATH)

clean:
	@echo clean
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	#@-rm $(APP_TARGET) $(targetdis) $(targetstrip)
	@-rm $(APP_TARGET) $(targetdis)
	@ls -l
