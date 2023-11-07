OFA_DIR = /usr/src/ofa_kernel/default
OFA_INCLUDE := $(OFA_DIR)/include
OFA_SYMVERS := $(OFA_DIR)/Module.symvers

ifneq ($(LINUXINCLUDE),)
# kbuild part of makefile

LINUXINCLUDE := \
  -I$(OFA_INCLUDE) \
  ${LINUXINCLUDE}

obj-m  := main_module.o
main_module-y := rdma_library.o main.o

else
# normal makefile
export KBUILD_EXTRA_SYMBOLS=$(OFA_SYMVERS)
KDIR = /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	rm -rf main_module.ko  main_module.mod.c  main_module.mod.o  main_module.o
	$(MAKE) -C $(KDIR) M=$$PWD clean

load:
	sudo insmod main_module.ko

unload:
	sudo rmmod -f main_module.ko
endif
