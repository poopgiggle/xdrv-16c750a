BASE_OBJS := src/drv/x-16c750.o src/drv/x-16c750_lld.o src/drv/x-16c750_fsm.o
CIRCBUFF_OBJS := src/circbuff/circbuff.o 
EDS_OBJS := src/eds/smp.o src/eds/dbg.o
obj-m += am335x_xuart.o
am335x_xuart-y := $(BASE_OBJS) $(EDS_OBJS) $(CIRCBUFF_OBJS) port/plat_omap2.o 

EXTRA_CFLAGS += -I$(PWD)/inc -Iinclude/xenomai 

all:
	make -C $(LINUX_SRC) M=$(PWD) modules
	
clean:
	make -C $(LINUX_SRC) M=$(PWD) clean
	
am335x:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(LINUX_SRC) M=$(PWD) 	\
		KBUILD_EXTRA_SYMBOLS=$(LINUX_SRC)/Module.symver modules
	
	
