CROSS_TARGET_PATH := /mnt/disk2/build_desktop_for_arm/output
OBJS := ze08.o ../../libraries/private/dbus/dbus-base.o ../../libraries/public/serial/serial.o
CFLAGS := -I../../libraries/private/logger -I../../libraries/private/dbus/inc -I../../libraries/private/dbus -I../../libraries/public/serial
TARGET := ze08_iot
LIBS := -L$(CROSS_TARGET_PATH)/target/usr/lib -ldbus-1 -lpthread -Wl,-rpath-link $(CROSS_TARGET_PATH)/target/usr/lib
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

.PHONY: clean install

install:
	cp -ar $(TARGET) ../../build/

clean:
	rm -rf *.o $(TARGET)