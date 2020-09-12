#ifndef __DBUS_IPC_NAME_H_
#define __DBUS_IPC_NAME_H_

//bus name可以看作是进程的标识符

#define COAP_BUS_NAME "iot.device.coap"
#define COAP_IFACE_PATH "iot.coap.method"
#define COAP_OBJ_PATH "/iot/coap"

#define ZE08_BUS_NAME "iot.device.ze08"
#define ZE08_IFACE_PATH "iot.ze08.method"
#define ZE08_OBJ_PATH "/iot/ze08"
#define ZE08_GET_METHOD_NAME "read"

#define SHTC1_BUS_NAME "iot.device.shtc1"
#define SHTC1_IFACE_PATH "iot.shtc1.method"
#define SHTC1_OBJ_PATH "/iot/shtc1"
#define SHTC1_GET_METHOD_TEMP "temperature"
#define SHTC1_GET_METHOD_HUMIDITY "humidity"
#define SHTC1_SET_TEMP_HITH_THRESHOLD "temp_high_threshold"

#define MQTT_CLIENT_BUS_NAME "iot.device.mqtt"

#define GPIO_BUS_NAME "iot.device.gpio"
#define GPIO_OBJ_PATH "/iot/gpio"
#define GPIO_IFACE_PATH "iot.gpio.method"
#define GPIO_SET_METHOD_NAME "SetGpioStatus"
#define GPIO_GET_METHOD_NAME "GetGpioStatus"




#endif
