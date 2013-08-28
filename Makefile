obj-m += am335x-xuart.o
am335x-xuart-y := src/x-16c750.o src/x-16c750_lld.o port/plat_omap2.o

EXTRA_CFLAGS += -I$(PWD)/src -I$(PWD)/inc -Iinclude/xenomai 

all:
	make -C $(LINUX_SRC) M=$(PWD) modules
	
clean:
	make -C $(LINUX_SRC) M=$(PWD) clean
	
am335x:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(LINUX_SRC) M=$(PWD) 	\
		KBUILD_EXTRA_SYMBOLS=$(LINUX_SRC)/Module.symver modules
	
	
