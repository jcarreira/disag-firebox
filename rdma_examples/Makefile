obj-m += client.o
ccflags-y=-I/usr/src/mlnx-ofed-kernel-3.2/include/ 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	sudo insmod client.ko

unload:
	sudo rmmod -f client.ko
 
clean:
	rm -rf server
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
