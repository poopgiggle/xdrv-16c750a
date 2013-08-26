obj-m += src/x-16c750.o

EXTRA_CFLAGS += -I$(PWD)/src -I$(PWD)/inc -Iinclude/xenomai

all:
	make -C $(LINUX_SRC) M=$(PWD) modules
	
clean:
	make -C $(LINUX_SRC) M=$(PWD) clean
	
tmod:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(LINUX_SRC) M=$(PWD) modules