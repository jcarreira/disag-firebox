#RMEM_LIBS := -lrdmacm -libverbs
ccflags-y=-I/usr/src/ofa_kernel/default/include/
#-I/usr/src/mlnx-ofed-kernel-3.1/include/ -I/usr/include/rdma/ -I/usr/lib/gcc/x86_64-linux-gnu/4.9/include -I/usr/local/include -I/usr/include -I/usr/src/mlnx-ofed-kernel-3.1/include/uapi/
#-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include-fixed -I/usr/include/x86_64-linux-gnu 

obj-m += my_mod.o 

ifndef PATH_KERNEL
PATH_KERNEL = /lib/modules/$(shell uname -r)/build
endif

all:
	make -C ${PATH_KERNEL} M=$(PWD) modules

clean:
	make -C ${PATH_KERNEL} M=$(PWD) clean
	rm *~ -f
