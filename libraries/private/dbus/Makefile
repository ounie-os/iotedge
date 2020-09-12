OBJS := dbus-base.o
CFLAGS := -I./inc -I../logger
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
TARGET := libdbus-base.so

all: $(OBJS)

.PHONY: clean

clean:
	rm -rf *.o
	