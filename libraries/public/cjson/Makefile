OBJS := cJSON.o
#CFLAGS := -fpic
CC := /opt/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
#TARGET := libcjson.so.1.7.12

all: $(OBJS)
	#$(CC) -shared $(OBJS) -o $(TARGET)
	#ln -sf $(TARGET) libcjson.so

.PHONY: clean

clean:
	rm -rf *.o *.so