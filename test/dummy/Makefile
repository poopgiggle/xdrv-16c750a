SRC = main.c

# Compiler settings 
PROGNAME = appl
TARGET = arm-linux-gnueabihf-
CC_PATH := /opt/bin
LD_SCRIPT_PATH := $(CC_PATH)/../arm-linux-gnueabihf/lib/ldscripts
CC := $(TARGET)gcc
LD := $(TARGET)ld

RM := rm -f

OBJ = $(SRC:.c=.o)

# Xenomai settings

CC_XENO_INCLUDE := -I$(LINUX_SRC)/include/xenomai -I$(LINUX_SRC)/xenomai-build/src/include
LD_XENO_INCLUDE = -L$(LINUX_SRC)/xenomai-build/src/skins/common/.libs
LD_XENO_INCLUDE += -L$(LINUX_SRC)/xenomai-build/src/skins/native/.libs
LD_XENO_INCLUDE += -L$(LINUX_SRC)/xenomai-build/src/skins/rtdm/.libs
LD_XENO_LIB = -lxenomai -lnative -lrtdm

# Generic setttings
CC_INCLUDE = -I$(LINUX_SRC)/usr/include -I$(LINUX_SRC)/arch/arm/include $(CC_XENO_INCLUDE)
CC_FLAGS = -mlittle-endian -marm -mabi=aapcs-linux -mno-thumb-interwork -march=armv7-a

LD_INCLUDE += -Wl,-rpath /opt/bin/../arm-linux-gnueabihf/lib $(LD_XENO_INCLUDE)
LD_LIB = -lc -lpthread $(LD_XENO_LIB) 
LD_FLAGS = 
LD_SCRIPT := $(LD_SCRIPT_PATH)/armelf_linux_eabi.x

all : $(PROGNAME).elf

%.elf :
	$(CC) $(CC_FLAGS) $(CC_INCLUDE) $(LD_FLAGS) $(LD_INCLUDE) $(LD_LIB) $(SRC) -o "$@"
	
%.o : %.c
	$(CC) $(CC_FLAGS) $(CC_INCLUDE) -c "$<" -o "$@"
	
clean :
	$(RM) $(OBJ) $(PROGNAME).elf
