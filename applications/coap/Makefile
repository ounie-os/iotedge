CROSS_TARGET_PATH := /mnt/disk2/build_desktop_for_arm/output
OBJS := coap_server_iot.o ../../libraries/private/dbus/dbus-base.o
CFLAGS := -I../../libraries/private/logger -I../../libraries/private/dbus/inc -I../../libraries/private/dbus -I../../libraries/public/libcoap/include
TARGET := coap_server_iot
LIBS := -L$(CROSS_TARGET_PATH)/target/usr/lib -ldbus-1 -lpthread -Wl,-rpath-link $(CROSS_TARGET_PATH)/target/usr/lib -I../../libraries/public/libcoap -lcoap-2-openssl 
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

.PHONY: clean install

install:
	cp -ar $(TARGET) ../../build/

clean:
	rm -rf *.o $(TARGET)