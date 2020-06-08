CROSS_TARGET_PATH := /mnt/disk2/build_desktop_for_arm/output
OBJS := gpio.o ../../libraries/private/dbus/dbus-base.o ../../libraries/private/gpio/gpio-api.o
CFLAGS := -I../../libraries/private/logger -I../../libraries/private/dbus/inc -I../../libraries/private/dbus -I../../libraries/private/gpio
TARGET := gpio-control
LIBS := -L$(CROSS_TARGET_PATH)/target/usr/lib -ldbus-1 -Wl,-rpath-link $(CROSS_TARGET_PATH)/target/usr/lib
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

.PHONY: clean install

install:
	cp -ar $(TARGET) ../../build/

clean:
	rm -rf *.o $(TARGET)