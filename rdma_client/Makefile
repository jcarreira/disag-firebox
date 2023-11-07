OFA_DIR = /usr/src/ofa_kernel/default
OFA_INCLUDE := $(OFA_DIR)/include
OFA_SYMVERS := $(OFA_DIR)/Module.symvers

obj-m += client.o
#ccflags-y=-I/usr/include/
ccflags-y=-I$(OFA_INCLUDE)
 
.PHONY: load unload

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	sudo insmod client.ko

unload:
	sudo rmmod -f client.ko
 
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
