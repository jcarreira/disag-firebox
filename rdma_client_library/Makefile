ifneq ($(KERNELRELEASE),)
    # kbuild part of makefile
obj-m  := main_module.o
main_module-y := rdma_library.o main.o
ccflags-y=-I/usr/src/mlnx-ofed-kernel-3.2/include/ -I./init

else
    KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	rm -rf main_module.ko  main_module.mod.c  main_module.mod.o  main_module.o
	$(MAKE) -C $(KDIR) M=$$PWD modules clean

load:
	sudo insmod main_module.ko

unload:
	sudo rmmod -f main_module.ko
endif
