COMPILE_SHARED_LIBRARY=y

DLIB_TARGET=libe3infra.so
SLIB_TARGET=libe3infra.a

HEADER=./include

DPDK_ARCH=x86_64-native-linuxapp-gcc

INCLUDE=$(INCLUDE_DIR)
INCLUDE += -I${RTE_SDK}/$(DPDK_ARCH)/include

CCFLAG+= -g3 -Werror -Wall -fpic -O3 -m64  -march=native
LDFLAG+= -L${RTE_SDK}/$(DPDK_ARCH)/lib
#EXTRA_OBJS = -lrte_mbuf
CC=gcc
LD=ld

SRC=$(wildcard src/*.c)
SRC+=$(wildcard client/*.c)

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
