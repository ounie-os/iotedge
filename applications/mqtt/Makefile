CROSS_TARGET_PATH := /mnt/disk2/build_desktop_for_arm/output
OBJS := mqtt-jetlinks-v1.o topic_process.o ../../libraries/private/dbus/dbus-base.o ../../libraries/public/cjson/cJSON.o ../../libraries/public/ssl/md5.o
CFLAGS := -I../../libraries/private/logger -I../../libraries/private/dbus/inc -I../../libraries/private/dbus -I../../libraries/public/cjson -I../../libraries/public/ssl -I$(CROSS_TARGET_PATH)/staging/usr/include
TARGET := mqtt-jetlinks-v1
LIBS := -L$(CROSS_TARGET_PATH)/target/usr/lib -ldbus-1 -lpaho-mqtt3a -lpthread -Wl,-rpath-link $(CROSS_TARGET_PATH)/target/usr/lib -L$(CROSS_TARGET_PATH)/target/lib -lcrypto
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

.PHONY: clean install

install:
	cp -ar $(TARGET) ../../build/

clean:
	rm -rf *.o $(TARGET)