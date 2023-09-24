KERNELDIR := /home/davidhsieh/work/C_workspace/alientek_linux/
TOOLCHAIN := ~/work/C_workspace/toolchain/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
CURRENT_PATH := $(shell pwd)

obj-m := gpioled.o
target = $(patsubst %.o,%.ko, $(obj-m))
targetdis = $(patsubst %.ko,%.dis, $(target))

kernel_modules:
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
	@$(TOOLCHAIN)objdump -dS $(target) > $(targetdis)

clean:
	@echo clean
	@$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	@-rm $(targetdis)
	@ls -l
