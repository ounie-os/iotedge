CROSS_TARGET_PATH := /mnt/disk2/build_desktop_for_arm/output
OBJS := md5.o
CFLAGS := -I$(CROSS_TARGET_PATH)/staging/usr/include
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

all: $(OBJS)

.PHONY: clean

clean:
	rm -rf *.o