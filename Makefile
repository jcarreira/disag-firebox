#RMEM_LIBS := -lrdmacm -libverbs
ccflags-y=-I/usr/src/mlnx-ofed-kernel-3.1/include/
#-I/usr/src/mlnx-ofed-kernel-3.1/include/ -I/usr/include/rdma/ -I/usr/lib/gcc/x86_64-linux-gnu/4.9/include -I/usr/local/include -I/usr/include -I/usr/src/mlnx-ofed-kernel-3.1/include/uapi/
#-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include-fixed -I/usr/include/x86_64-linux-gnu 

obj-m += disag_mod.o register_client_mod.o

.PHONY: copy_symvers load unload

ifndef PATH_KERNEL
PATH_KERNEL = /lib/modules/$(shell uname -r)/build
endif

all:
	make -C ${PATH_KERNEL} M=$(PWD) modules

copy_symvers:
	cp /usr/src/ofa_kernel/default/Module.symvers .

load:
	sudo insmod disag_mod.ko

unload:
	sudo rmmod -f disag_mod.ko


clean:
	make -C ${PATH_KERNEL} M=$(PWD) clean
	rm *~ -f
