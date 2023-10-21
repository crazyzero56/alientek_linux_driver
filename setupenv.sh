#!/bin/sh

echo "setup toolchain"
export PATH=$PATH:/home/davidhsieh/work/C_workspace/toolchain/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/
#echo "please key : make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- stm32mp157d_atk_defconfig"
#echo "make V=1 ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- DEVICE_TREE=stm32mp157d-atk all"
#echo "make uImage dtbs LOADADDR=0XC2000040 -j16"
