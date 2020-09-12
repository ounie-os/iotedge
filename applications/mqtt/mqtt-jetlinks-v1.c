#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "md5.h"
#include "MQTTAsync.h"
#include "cJSON.h"
#include "dbus-base.h"
#include "logger.h"
#include "topic_process.h"
#include "dbus_ipc_name.h"
#include <pthread.h>



#define ADDRESS     "tcp://192.168.1.188:1883"
#define CLIENTID    "air-quality-1" // 设备ID

#define QOS         0

#define CH2O_WARNING_ID "ch2o_warning"
#define PRODUCT_ID "13816161818"
#define DEVICE_ID CLIENTID

typedef struct
{
    MQTTAsync client;
    DBusConnection *conn;
}mqtt_msg_data_t;

typedef struct
{
    char productId[32];
    char deviceId[32];
}dev_model_info_t;

static dev_model_info_t the_model;

static DBusHandlerResult mqtt_dbus_msg_handle(DBusConnection *c, DBusMessage *m, void *data);
void mqtt_report_event(MQTTAsync client, const char *id, const char *param);
static FilterFuncsCallback filter_handle = {
            .message_handler = mqtt_dbus_msg_handle,
            .free_func_handler = NULL,
            .data = NULL};

static int disc_finished = 0;
static int subscribed = 0;
static int finished = 0;
static int connectFail = 0;

unsigned long long get_timestamp_now(void)
{
    struct timeval stamp;
    unsigned long long stamp_ms = 0;
    gettimeofday(&stamp, NULL);
    stamp_ms = stamp.tv_sec;
    stamp_ms *= 1000;
    return stamp_ms;
}

static void cfinish(int sig)
{
    signal(SIGINT, NULL);
    printf("sig %d got\n", sig);
    //disc_finished = 1;
}

static DBusHandlerResult mqtt_dbus_msg_handle(DBusConnection *c, DBusMessage *m, void *data)
{
    const char *iface_path, *member_name, *obj_path;
    int type;
    MQTTAsync client = (MQTTAsync)data;
    char buf[256] = {0};
    char *param = NULL;
    iface_path = dbus_message_get_interface(m);
    member_name = dbus_message_get_member(m);
    obj_path = dbus_message_get_path(m);
    type = dbus_message_get_type(m);
    dbg(IOT_DEBUG, "if:%s, method:%s, obj:%s, type:%d", iface_path, member_name, obj_path, type);
    if ((NULL == iface_path) || (NULL == member_name) || (NULL == obj_path))
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (0 == strcmp(iface_path, ZE08_IFACE_PATH))
    {
        if (0 == strcmp(member_name, "ch2o_warning"))
        {
            get_single_arg_from_message(m, &param);
            if (NULL != param)
            {
                dbg(IOT_DEBUG, "recv ch2o value is %s", param);
                mqtt_report_event(client, CH2O_WARNING_ID, param);
            }
        }
    }
    else if (0 == strcmp(iface_path, SHTC1_IFACE_PATH))
    {
        if (0 == strcmp(member_name, "temp_over"))
        {
            get_single_arg_from_message(m, &param);
            if (NULL != param)
            {
                dbg(IOT_DEBUG, "recv temp value is %s", param);
                mqtt_report_event(client, "temp_over", param);
            }
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

// /{productId}/{deviceId}/event/{eventId}
void mqtt_report_event(MQTTAsync client, const char *eventId, const char *param)
{
    //mqtt_pubmsg();
    char topicBuf[128] = {0}, payloadBuf[128] = {0};
    const char *bodyTopic = "/%s/%s/event/%s";
    const char *payloadTopic = "{\"messageId\":%d, \"data\":%s}";
    snprintf(topicBuf, sizeof(topicBuf), bodyTopic, the_model.productId, the_model.deviceId, eventId);
    snprintf(payloadBuf, sizeof(payloadBuf), payloadTopic, 188, param);
    dbg(IOT_DEBUG, "report topic is %s", topicBuf);
    dbg(IOT_DEBUG, "report payload is %s", payloadBuf);
    mqtt_pubmsg(client, topicBuf, payloadBuf);
}

int gpio_set(DBusConnection *conn, int pin, int on_off)
{
    IpcPath dst_path_set = {
            .bus_path = GPIO_BUS_NAME,
            .obj_path = GPIO_OBJ_PATH,
            .interface_path = GPIO_IFACE_PATH,
            .act.method_name = GPIO_SET_METHOD_NAME
        };

    const char *gpio_status[2] = {"0", "1"};
    char *gpio_value_now = NULL;

    return send_method_call_without_reply(conn, &dst_path_set, DBUS_TYPE_STRING, &gpio_status[on_off]);
}

static int shtc1_set_temp_high_threshold(DBusConnection *conn, char* threshold)
{
    
    IpcPath dst_path_set = {
            .bus_path = SHTC1_BUS_NAME,
            .obj_path = SHTC1_OBJ_PATH,
            .interface_path = SHTC1_IFACE_PATH,
            .act.method_name = SHTC1_SET_TEMP_HITH_THRESHOLD
        };


    return send_method_call_without_reply(conn, &dst_path_set, DBUS_TYPE_STRING, (const void*)&threshold);
}


static int gpio_status_publish(DBusConnection *conn, void *context, int pin, const char *topic, MQTTAsync_message *message)
{
    MQTTAsync client = (MQTTAsync)context;
    const char *payload;
    int on_off, rc;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    char *gpio_value = NULL;
    cJSON *json = NULL;
    char *messageId = NULL;
    char buf[256] = {0};
    unsigned long stamp;

    IpcPath dst_path_set = {
            .bus_path = GPIO_BUS_NAME,
            .obj_path = GPIO_OBJ_PATH,
            .interface_path = GPIO_IFACE_PATH,
            .act.method_name = GPIO_GET_METHOD_NAME
        };
    send_method_call_with_reply(conn, &dst_path_set, NULL, &gpio_value);
    dbg(IOT_DEBUG, "gpio value is %s", gpio_value);
    if (NULL == message) // 属性上报
    {
        const char *body = "{\"messageId\":\"%s\", \"properties\":[\"leds_status\":%s]}";
        snprintf(buf, sizeof(buf), body, "1", gpio_value);
    }
    else // 属性回复
    {
        const char *body = "{\"messageId\":\"%s\", \"properties\":{\"leds_status\":\"%s\"}, \"deviceId\":\"%s\", \"success\":true}";
        json = cJSON_Parse(message->payload);
        messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
        dbg(IOT_DEBUG, "message id %s", messageId);
        //stamp = get_timestamp();
        snprintf(buf, sizeof(buf), body, messageId, gpio_value, CLIENTID);
        
    }
    mqtt_pubmsg(client, topic, buf);
    
    if (json)
    {
        cJSON_Delete(json);
    }
    return rc;
}

static int ze08_status_publish(DBusConnection *conn, MQTTAsync client, const char *topic, MQTTAsync_message *message)
{
    const char *payload;
    int on_off, rc;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    char *ch2o_value = NULL;
    cJSON *json = NULL;
    char *messageId = NULL;
    char buf[256] = {0};
    unsigned int stamp;
    const char *body = "{\"messageId\":\"%s\", \"deviceId\":\"%s\", \"success\":true, \"properties\":{\"ch2o_value\":\"%s\"}}";

    IpcPath dst_path_set = {
            .bus_path = ZE08_BUS_NAME,
            .obj_path = ZE08_OBJ_PATH,
            .interface_path = ZE08_IFACE_PATH,
            .act.method_name = ZE08_GET_METHOD_NAME
        };
    send_method_call_with_reply(conn, &dst_path_set, NULL, &ch2o_value);
    dbg(IOT_DEBUG, "ch2o value is %s", ch2o_value);
    json = cJSON_Parse(message->payload);
    messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
    dbg(IOT_DEBUG, "message id %s", messageId);
    snprintf(buf, sizeof(buf), body, messageId, CLIENTID, ch2o_value);
    mqtt_pubmsg(client, topic, buf);
    
    if (json)
    {
        cJSON_Delete(json);
    }
    return rc;
}

static int shtc1_temp_publish(DBusConnection *conn, MQTTAsync client, const char *topic, MQTTAsync_message *message)
{
    int rc = 0;
    char *temp_value = NULL;
    cJSON *json = NULL;
    char *messageId = NULL;
    char buf[256] = {0};
    const char *body = "{\"messageId\":\"%s\", \"deviceId\":\"%s\", \"success\":true, \"properties\":{\"temperature\":\"%s\"}}";

    IpcPath dst_path_set = {
            .bus_path = SHTC1_BUS_NAME,
            .obj_path = SHTC1_OBJ_PATH,
            .interface_path = SHTC1_IFACE_PATH,
            .act.method_name = SHTC1_GET_METHOD_TEMP
         };

    send_method_call_with_reply(conn, &dst_path_set, NULL, &temp_value);
    dbg(IOT_DEBUG, "temperature value is %s", temp_value);
    json = cJSON_Parse(message->payload);
    messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
    dbg(IOT_DEBUG, "message id %s", messageId);
    snprintf(buf, sizeof(buf), body, messageId, CLIENTID, temp_value);
    mqtt_pubmsg(client, topic, buf);

    if (json)
    {
        cJSON_Delete(json);
    }
    return rc;     
}

static int shtc1_humidity_publish(DBusConnection *conn, MQTTAsync client, const char *topic, MQTTAsync_message *message)
{
    char *humidity_value = NULL;
    cJSON *json = NULL;
    int rc = 0;
    char *messageId = NULL;
    char buf[256] = {0};

    const char *body = "{\"messageId\":\"%s\", \"deviceId\":\"%s\", \"success\":true, \"properties\":{\"humidity\":\"%s\"}}";

    IpcPath dst_path_set = {
            .bus_path = SHTC1_BUS_NAME,
            .obj_path = SHTC1_OBJ_PATH,
            .interface_path = SHTC1_IFACE_PATH,
            .act.method_name = SHTC1_GET_METHOD_HUMIDITY
     };

    send_method_call_with_reply(conn, &dst_path_set, NULL, &humidity_value);
    dbg(IOT_DEBUG, "humidity value is %s", humidity_value);
    json = cJSON_Parse(message->payload);
    messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
    dbg(IOT_DEBUG, "message id %s", messageId);
    snprintf(buf, sizeof(buf), body, messageId, CLIENTID, humidity_value);
    mqtt_pubmsg(client, topic, buf);

    if (json)
    {
        cJSON_Delete(json);
    }
    return rc;   
}


static void get_properties(DBusConnection *conn, MQTTAsync client)
{
    char *humidity_value = NULL;
    char *temp_value = NULL;
    char *ch2o_value = NULL;
    char humidity_buf[8] = {0};
    char temp_buf[8] = {0};
    char ch2o_buf[8] = {0};
    char buf[256] = {0};
    const char *body = "{\"messageId\":\"188\",\"properties\":{\"humidity\":\"%s\", \"temperature\":\"%s\", \"ch2o_value\":\"%s\"}}";

    IpcPath dst_path_set_humidity = {
            .bus_path = SHTC1_BUS_NAME,
            .obj_path = SHTC1_OBJ_PATH,
            .interface_path = SHTC1_IFACE_PATH,
            .act.method_name = SHTC1_GET_METHOD_HUMIDITY
     };

    IpcPath dst_path_set_temp = {
        .bus_path = SHTC1_BUS_NAME,
        .obj_path = SHTC1_OBJ_PATH,
        .interface_path = SHTC1_IFACE_PATH,
        .act.method_name = SHTC1_GET_METHOD_TEMP
    };

    IpcPath dst_path_set_ze08 = {
            .bus_path = ZE08_BUS_NAME,
            .obj_path = ZE08_OBJ_PATH,
            .interface_path = ZE08_IFACE_PATH,
            .act.method_name = ZE08_GET_METHOD_NAME
    };
    /*
        send_method_call_with_reply 返回的指针的内容需要在再次调用前把内容拷贝出来
        否则就会被后续内容覆盖
    */
    send_method_call_with_reply(conn, &dst_path_set_ze08, NULL, &ch2o_value);
    snprintf(ch2o_buf, sizeof(ch2o_buf)-1, "%s", ch2o_value);

    send_method_call_with_reply(conn, &dst_path_set_temp, NULL, &temp_value);
    snprintf(temp_buf, sizeof(temp_buf)-1, "%s", temp_value);

    send_method_call_with_reply(conn, &dst_path_set_humidity, NULL, &humidity_value);
    snprintf(humidity_buf, sizeof(humidity_buf)-1, "%s", humidity_value);

    snprintf(buf, sizeof(buf), body, humidity_buf, temp_buf, ch2o_buf);
    //printf("%s\n", buf);

    mqtt_pubmsg(client, "/13816161818/"CLIENTID"/properties/report", buf);
}

static void *get_properties_process(void *arg)
{
    mqtt_msg_data_t *p_msg_data_context = (mqtt_msg_data_t *)arg;
    while (!disc_finished)
    {
        get_properties(p_msg_data_context->conn, p_msg_data_context->client);
        sleep(3);
        //printf("=== start get_properties_process ===\n");
    }
}


void onsubscribe(void* context, MQTTAsync_successData* response)
{
#if 0
    MQTTAsync *client = (MQTTAsync *)context;
	
	subscribed = 1;
#endif /* 0 */

    /** 订阅成功后，向服务端publish状态信息 */
	//gpio_status_publish(the_conn, client, 53, GPIO_UPLOAD_TOPIC, NULL);
}

void onsubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	dbg(IOT_WARNING, "Subscribe failed, rc %d\n", response ? response->code : 0);
}

void onconnect(void* context, MQTTAsync_successData* response)
{
#if 0
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;
#endif /* 0 */

	dbg(IOT_DEBUG, "Successful connection");
	finished = 1;

#if 0
	dbg(IOT_DEBUG, "Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n", TOPIC, CLIENTID, QOS);
	opts.onSuccess = onsubscribe;
	opts.onFailure = onsubscribeFailure;
	opts.context = client;

	if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{
		dbg(IOT_ERROR, "Failed to start subscribe, return code %d", rc);
	}
#endif /* 0 */
}

int mqtt_pubmsg(MQTTAsync client, const char *topic, const char *payload)
{
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    int rc;

    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    dbg(IOT_DEBUG, "reply message %s", payload);
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, NULL)) != MQTTASYNC_SUCCESS)
    {
        dbg(IOT_WARNING, "Failed to publish message, return code %d\n", rc);
    }    
    return rc;
}

static void mqtt_msg_dispatch(DBusConnection *dbus_conn, MQTTAsync client, char *topicName, int topicLen, MQTTAsync_message *message)
{
    cJSON *json, *json_properties, *json_properties_iter, *json_inputs, *json_inputs_iter;
    int properties_size, inputs_size;
    int i;
    char *properties_name, *function_name, *messageId, *key, *value;
    char buf[256] = {0};
    topic_item_t producdIdTopic, deviceIdTopic, modelActTopic, propertiesAct;
    char *action;
    char productId[32] = {0}, deviceId[32] = {0}, act[32] = {0};
    int productIdLen, deviceIdLen, actModelLen;
    char topic_reply[128] = {0};
    
    topic_product_id_jetlinks(topicName, topicLen, &producdIdTopic);
    topic_device_id_jetlinks(topicName, topicLen, &deviceIdTopic);
    productIdLen = (producdIdTopic.len >= sizeof(productId)) ? sizeof(productId)-1 : producdIdTopic.len;
    strncpy(productId, producdIdTopic.start, productIdLen);
    dbg(IOT_DEBUG, "productId is %s", productId);
    deviceIdLen = (deviceIdTopic.len >= sizeof(deviceId)) ? sizeof(deviceId)-1 : deviceIdTopic.len;
    strncpy(deviceId, deviceIdTopic.start, deviceIdLen);
    dbg(IOT_DEBUG, "deviceId is %s", deviceId);

    // /product-id/device-id/${action}
    action = topic_model_act_jetlinks(topicName, topicLen, &modelActTopic);
    actModelLen = modelActTopic.len >= sizeof(act) ? sizeof(act)-1 : modelActTopic.len;
    strncpy(act, modelActTopic.start, actModelLen);
    topic_operation_act_jetlinks(action, strlen(action), &propertiesAct);

    json = cJSON_Parse(message->payload);

    if (0 == strcmp(act, "properties")) // 设备属性操作
    {
        if (0 == strncmp(propertiesAct.start, "read", 4))
        {
            printf("properties read\n");
            strncpy(topic_reply, topicName, (strlen(topicName)+1) > sizeof(topic_reply) ? sizeof(topic_reply)-1 : strlen(topicName));
            strncat(topic_reply, "/reply", 6);
            json_properties = cJSON_GetObjectItem(json, "properties");
            dbg(IOT_DEBUG, "properties type is %d", json_properties->type);
            properties_size = cJSON_GetArraySize(json_properties);
            for (i=0; i<properties_size; i++)
            {
                json_properties_iter = cJSON_GetArrayItem(json_properties, i);
                properties_name = json_properties_iter->valuestring;
                dbg(IOT_DEBUG, "property name %s", properties_name);
                if (0 == strcmp(properties_name, "leds_status"))
                {
                    gpio_status_publish(dbus_conn, client, 53, topic_reply, message);
                }
                else if (0 == strcmp(properties_name, "ch2o_value"))
                {
                    ze08_status_publish(dbus_conn, client, topic_reply, message);
                }
                else if (0 == strcmp(properties_name, "temperature"))
                {
                    shtc1_temp_publish(dbus_conn, client, topic_reply, message);
                }
                else if (0 == strcmp(properties_name, "humidity"))
                {
                    shtc1_humidity_publish(dbus_conn, client, topic_reply, message);
                }
            }
        }
        else if (0 == strncmp(propertiesAct.start, "write", 5))
        {
            printf("properties write\n");
        }
    }
    else if (0 == strcmp(act, "function")) //设备功能调用
    {
        if (0 == strncmp(propertiesAct.start, "invoke", 6))
        {
            printf("function invoke\n");
            strncpy(topic_reply, topicName, (strlen(topicName)+1) > sizeof(topic_reply) ? sizeof(topic_reply)-1 : strlen(topicName));
            strncat(topic_reply, "/reply", 6);
            const char *body = "{\"messageId\":\"%s\", \"output\": \"success\", \"success\":true}";
            function_name = cJSON_GetObjectItem(json, "function")->valuestring;
            
            if (0 == strcmp(function_name, "turn_off_led"))
            {
                gpio_set(dbus_conn, 53, 0);
            }
            else if (0 == strcmp(function_name, "turn_on_led"))
            {
                gpio_set(dbus_conn, 53, 1);
            }
            else if (0 == strcmp(function_name, "set_temp_high_threshold"))
            {
                json_inputs = cJSON_GetObjectItem(json, "inputs");
                inputs_size = cJSON_GetArraySize(json_inputs);
                for (i = 0; i < inputs_size; i++)
                {
                    json_inputs_iter = cJSON_GetArrayItem(json_inputs, i);
                    key = cJSON_GetObjectItem(json_inputs_iter, "name")->valuestring;
                    dbg(IOT_DEBUG, "name = %s", key);
                    value = cJSON_GetObjectItem(json_inputs_iter, "value")->valuestring;
                    dbg(IOT_DEBUG, "value = %f", value);
                    if (0 == strcmp(key, "high_threshold"))
                    {
                        shtc1_set_temp_high_threshold(dbus_conn, value);
                    }
                }
            }
            messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
            //deviceId = cJSON_GetObjectItem(json, "deviceId")->valuestring;
            snprintf(buf, sizeof(buf), body, messageId);
            mqtt_pubmsg(client, topic_reply, buf);

        }
    }

#if 0
    if (0 == strcmp(topicName, "/invoke-function"))
    {
        const char *body = "{\"messageId\":\"%s\", \"deviceId\":\"%s\", \"output\": \"ok\", \"success\":true}";
        function_name = cJSON_GetObjectItem(json, "function")->valuestring;
        
        if (0 == strcmp(function_name, "turn_off_led"))
        {
            gpio_set(dbus_conn, 53, 0);
        }
        else if (0 == strcmp(function_name, "turn_on_led"))
        {
            gpio_set(dbus_conn, 53, 1);
        }
        messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
        //deviceId = cJSON_GetObjectItem(json, "deviceId")->valuestring;
        snprintf(buf, sizeof(buf), body, messageId, deviceId);
        mqtt_pubmsg(client, topicName, buf);
        
    }
    else if (0 == strcmp(topicName, "/read-property"))
    {
        json_properties = cJSON_GetObjectItem(json, "properties");
        dbg(IOT_DEBUG, "properties type is %d", json_properties->type);
        properties_size = cJSON_GetArraySize(json_properties);
        for (i=0; i<properties_size; i++)
        {
            json_properties_iter = cJSON_GetArrayItem(json_properties, i);
            properties_name = json_properties_iter->valuestring;
            dbg(IOT_DEBUG, "property name %s", properties_name);
            if (0 == strcmp(properties_name, "leds_status"))
            {
                gpio_status_publish(dbus_conn, client, 53, topicName, message);
            }
            else if (0 == strcmp(properties_name, "ch2o_value"))
            {
                ze08_status_publish(dbus_conn, client, topicName, message);
            }
        }
    }
#endif /* 0 */
    else
    {
        dbg(IOT_WARNING, "no match topic to be processed");
    }
    
    cJSON_Delete(json);
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
    char* payloadptr;
	mqtt_msg_data_t *p = (mqtt_msg_data_t *)context;
	MQTTAsync client = p->client;
	DBusConnection *the_conn = p->conn;

    dbg(IOT_DEBUG, "Message arrived");
    dbg(IOT_DEBUG, "     topic: %s", topicName);
    dbg(IOT_DEBUG, "   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');

    mqtt_msg_dispatch(the_conn, client, topicName, topicLen, message);

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
	mqtt_msg_data_t *p = (mqtt_msg_data_t *)context;
	MQTTAsync client = p->client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	dbg(IOT_DEBUG, "Connection lost");
	if (cause)
		dbg(IOT_DEBUG, "     cause: %s", cause);

	dbg(IOT_DEBUG, "Reconnecting");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		dbg(IOT_WARNING, "Failed to start connect, return code %d", rc);
	}
}

void onconnectFailure(void* context, MQTTAsync_failureData* response)
{
	dbg(IOT_WARNING, "Connect failed, rc %d (%s)", response ? response->code : 0, response ? response->message : NULL);
	connectFail = 1;
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	dbg(IOT_WARNING, "Successful disconnection\n");
	disc_finished = 1;
}

static int mqtt_start_connect(void *data)
{
    unsigned long long auth_time_stamp;
	char username[128], passwd[128];
	unsigned char md5_passwd[16];
	char md5_passwd_buf[64] = {0};
	int i, rc = 0;
	mqtt_msg_data_t *p_msg_data_context = (mqtt_msg_data_t *)data;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	
    auth_time_stamp = get_timestamp_now();
    snprintf(username, sizeof(username), "%s|%llu", "led1", auth_time_stamp);
    dbg(IOT_DEBUG, "username = %s", username);
    snprintf(passwd, sizeof(passwd), "%s|%llu|%s", "led1", auth_time_stamp, "led1");
    dbg(IOT_DEBUG, "origin passwd is %s", passwd);
    transform_to_md5(passwd, strlen(passwd), md5_passwd);

    for (i=0; i<16; i++)
    {
        snprintf(md5_passwd_buf + i*2, 3, "%02x", md5_passwd[i]);
    }
    dbg(IOT_DEBUG, "md5 passwd is %s", md5_passwd_buf);

    conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = username;
	conn_opts.password = md5_passwd_buf;
	conn_opts.onSuccess = onconnect;
	conn_opts.onFailure = onconnectFailure;
	conn_opts.context = p_msg_data_context;

	if ((rc = MQTTAsync_connect(p_msg_data_context->client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
	}
	return rc;
}


static int mqtt_network_conf(const char* conf_path, char *address_buf, int len)
{
    FILE *eth0_conf;
    eth0_conf = fopen(conf_path, "r");
    if (NULL == eth0_conf)
    {
        dbg(IOT_WARNING, "need network conf file");
        while (1)
        {
            sleep(2);
            eth0_conf = fopen(conf_path, "r");
            if (NULL != eth0_conf)
            {
                break;
            }
        }
    }
    if (NULL == fgets(address_buf, len, eth0_conf))
    {
        return -1;
    }
    dbg(IOT_DEBUG, "mqtt network conf = %s", address_buf);

    fclose(eth0_conf);

    return 0;
}

int main(int argc, char* argv[])
{
    FdWatchMap *poll_data;
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	int rc;
	int ret;
	mqtt_msg_data_t msg_data_context;
	DBusConnection *the_conn;
	unsigned long long auth_time_stamp;
	char username[128], passwd[128];
	unsigned char md5_passwd[16];
	char md5_passwd_buf[64] = {0};
	int i;
	char address_buf[32] = {0};
	pthread_t ntid;
	
    signal(SIGINT, cfinish);
    strncpy(the_model.productId, PRODUCT_ID, strlen(PRODUCT_ID)+1);
    strncpy(the_model.deviceId, DEVICE_ID, strlen(DEVICE_ID)+1);

    the_conn = connect_dbus(MQTT_CLIENT_BUS_NAME, &rc);
    if (NULL == the_conn)
    {
        dbg(IOT_ERROR, "connect_dbus fail, rc = %d", rc);
        return 1;
    }

    poll_data = register_watch_functions_default(the_conn);
    if (NULL == poll_data)
    {
        dbg(IOT_ERROR, "Failed to register watch handler callback");
        return 1;
    }

    // 订阅iot.ze08发来的广播信息
    dbus_add_match(the_conn, "type='signal', interface="ZE08_IFACE_PATH);
    // 订阅iot.shtc1发来的广播信息
    dbus_add_match(the_conn, "type='signal', interface="SHTC1_IFACE_PATH);

    mqtt_network_conf("/mnt/ifcfg-mqtt", address_buf, 32);

    ret = MQTTAsync_create(&client, address_buf, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != 0)
    {
        dbg(IOT_ERROR, "MQTTAsync_create return %d\n", ret);
        dbus_connection_unref(the_conn);
        return 1;
    }

    msg_data_context.client = client;
    msg_data_context.conn = the_conn;
    
    rc = MQTTAsync_setCallbacks(client, &msg_data_context, connlost, msgarrvd, NULL);
    if (ret != 0)
        printf("MQTTAsync_setCallbacks return %d\n", ret);

    mqtt_start_connect(&msg_data_context);
    while (!finished)
    {
        if (connectFail)
        {
            mqtt_start_connect(&msg_data_context);
            connectFail = 0;
        }
        sleep(1);
    }    

	filter_handle.data = client;

	ret = register_filter_functions(the_conn, &filter_handle);
    if (ret == FALSE)
    {
        dbg(IOT_ERROR, "Failed to register msg handler callback");
        return 1;
    }

    rc = pthread_create(&ntid, NULL, get_properties_process, (void*)&msg_data_context);
    if (rc != 0)
    {
        dbg(IOT_ERROR, "pthread create fail");
    }

    dbus_loop_start(the_conn, (void *)poll_data);

    disc_opts.onSuccess = onDisconnect;
    if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start disconnect, return code %d\n", rc);
        disc_finished = 1;
    }
    
    while (!disc_finished)
    {
        usleep(10000L);
    }

exit:
    dbg(IOT_WARNING, "go to exit");
    MQTTAsync_destroy(&client);
    return rc;
}



