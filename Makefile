obj-m += module.o

ifndef PATH_KERNEL
PATH_KERNEL = /lib/modules/$(shell uname -r)/build # this is my kernel's path
endif

all:
	make -C ${PATH_KERNEL} M=$(PWD) modules

clean:
	make -C ${PATH_KERNEL} M=$(PWD) clean
	rm *~ -f
