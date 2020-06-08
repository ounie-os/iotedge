CROSS_TARGET_PATH := /mnt/disk2/build_desktop_for_arm/output
OBJS := $(addprefix src/, $(patsubst %.c, %.o, $(filter %.c, $(shell ls $(CURDIR)/src))))
CFLAGS := -fpic -I./include/coap2 -D_GNU_SOURCE -I$(CROSS_TARGET_PATH)/staging/usr/include
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
TARGET := libcoap-2-openssl.so

all: $(OBJS)
	$(CC) -shared $(OBJS) -o $(TARGET)


.PHONY: clean

clean:
	rm -rf src/*.o *.so