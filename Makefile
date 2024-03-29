# DIR := $(shell ls -l | grep ^d | awk '{print $$9}')

DBUS_PATH := libraries/private/dbus
GPIO_PATH := libraries/private/gpio
CJSON_PATH := libraries/public/cjson
SERIAL_PATH := libraries/public/serial
SSL_PATH := libraries/public/ssl
COAP_PATH := libraries/public/libcoap
CONFIG_PATH := libraries/public/config
APP_COAP_PATH := applications/coap
APP_GPIO_PATH := applications/gpio
APP_MODBUS_PATH := applications/modbus
APP_MQTT_PATH := applications/mqtt
APP_ZE08_PATH := applications/ze08
APP_SHTC1_PATH := applications/shtc1

ALL_PATH := $(DBUS_PATH) $(GPIO_PATH) $(CJSON_PATH) $(CONFIG_PATH) $(SERIAL_PATH) $(SSL_PATH) $(COAP_PATH) $(APP_COAP_PATH) $(APP_GPIO_PATH) $(APP_MODBUS_PATH) $(APP_MQTT_PATH) $(APP_ZE08_PATH) $(APP_SHTC1_PATH)

define MAKE_TARGET_TEMPLATE
	make -C $1 $2;
endef

all: dbus-base gpio-api cjson-api config-api serial-api ssl-api coap-api coap-app gpio-app modbus-app mqtt-app ze08-app shtc1-app

dbus-base:
	#make -C $(DBUS_PATH) all
	$(call MAKE_TARGET_TEMPLATE, $(DBUS_PATH), all)

gpio-api:
	$(call MAKE_TARGET_TEMPLATE, $(GPIO_PATH), all)

cjson-api:
	$(call MAKE_TARGET_TEMPLATE, $(CJSON_PATH), all)

config-api:
	$(call MAKE_TARGET_TEMPLATE, $(CONFIG_PATH), all)

serial-api:
	$(call MAKE_TARGET_TEMPLATE, $(SERIAL_PATH), all)

ssl-api:
	$(call MAKE_TARGET_TEMPLATE, $(SSL_PATH), all)

coap-api:
	$(call MAKE_TARGET_TEMPLATE, $(COAP_PATH), all)

coap-app:
	$(call MAKE_TARGET_TEMPLATE, $(APP_COAP_PATH), all)

gpio-app:
	$(call MAKE_TARGET_TEMPLATE, $(APP_GPIO_PATH), all)

modbus-app:
	$(call MAKE_TARGET_TEMPLATE, $(APP_MODBUS_PATH), all)

mqtt-app:
	$(call MAKE_TARGET_TEMPLATE, $(APP_MQTT_PATH), all)

ze08-app:
	$(call MAKE_TARGET_TEMPLATE, $(APP_ZE08_PATH), all)

shtc1-app:
	$(call MAKE_TARGET_TEMPLATE, $(APP_SHTC1_PATH), all)

.PHONY: clean install

install:

clean:
	$(foreach path, $(ALL_PATH), $(call MAKE_TARGET_TEMPLATE, $(path), clean))