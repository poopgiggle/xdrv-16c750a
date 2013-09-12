M_BASE_OBJS     := src/drv/x-16c750.o src/drv/x-16c750_lld.o
M_CIRCBUFF_OBJS := src/circbuff/circbuff.o 

M_PORT_ARCH 	:= arm
M_PORT_PLAT 	:= $(M_PORT_ARCH)/omap2

M_PORT_OBJS 	:= port/$(M_PORT_PLAT)/plat_omap2.o
M_PORT_INCLUDE  := $(M_PORT_ARCH)

am335x-xuart-y  := $(M_BASE_OBJS) $(M_CIRCBUFF_OBJS) $(M_PORT_OBJS)
obj-m           += am335x-xuart.o

C_INCLUDE 		:= -I$(PWD) -I$(PWD)/inc -I$(PWD)/port/$(M_PORT_INCLUDE) -Iinclude/xenomai 
EXTRA_CFLAGS    += $(C_INCLUDE)

all: am335x
	make -C $(LINUX_SRC) M=$(PWD) modules
	
clean:
	make -C $(LINUX_SRC) M=$(PWD) clean
	
am335x:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(LINUX_SRC) M=$(PWD) 	\
		KBUILD_EXTRA_SYMBOLS=$(LINUX_SRC)/Module.symver modules
	
	
