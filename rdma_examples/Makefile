obj-m += client.o
ccflags-y=-I/usr/src/mlnx-ofed-kernel-3.2/include/ 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	rm -rf server
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
