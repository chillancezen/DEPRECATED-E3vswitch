COMPILE_SHARED_LIBRARY=y

DLIB_TARGET=libe3datapath.so 
SLIB_TARGET=libe3datapath.a 

HEADER=./include

DPDK_ARCH=x86_64-native-linuxapp-gcc

INCLUDE=$(INCLUDE_DIR)
INCLUDE += -I${RTE_SDK}/$(DPDK_ARCH)/include -include rte_config.h
CCFLAG += -D_LGPL_SOURCE
CCFLAG+= -g3 -Wall -fpic -O3 -m64  -march=native
LDFLAG+= -L${RTE_SDK}/$(DPDK_ARCH)/lib
CC=gcc
LD=ld

SRC=$(wildcard e3infra/*.c)
SRC+= $(wildcard e3net/*.c)

OBJ=$(patsubst %.c,%.o,$(SRC))
ifeq ($(COMPILE_SHARED_LIBRARY),y)
TARGET=$(DLIB_TARGET)
else 
TARGET=$(SLIB_TARGET)
endif

%.o:%.c
	$(CC) -I$(HEADER) $(INCLUDE)  $(CCFLAG) -c -o $@ $<

$(TARGET):$(OBJ)
ifeq ($(TARGET),$(DLIB_TARGET))
	gcc -shared  $(LDFLAG) -o $(DLIB_TARGET)  $(OBJ)
else
	ar cr $(SLIB_TARGET) $(OBJ)
endif

install:$(DLIB_TARGET)
	rm -f /lib64/$(DLIB_TARGET)
	cp ./$(DLIB_TARGET) /lib64
uninstall:$(DLIB_TARGET)
	-rm -f /lib64/$(DLIB_TARGET)
clean:
	-rm -f $(TARGET)
	find -name "*.o" -exec rm -f {} \;
