#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "MQTTAsync.h"
#include "cJSON.h"
#include "dbus-base.h"
#include "logger.h"


#define ADDRESS     "tcp://192.168.1.188:1883"
#define CLIENTID    "gateway-1"

#define TOPIC       "/read-property"
#define QOS         0
#define GPIO_UPLOAD_TOPIC  "/led_status"
#define GPIO_STATUS_TOPIC "/read-property"

#define MQTT_AGENT_BUS_NAME "iot.device.mqtt"

typedef struct
{
    MQTTAsync client;
    DBusConnection *conn;
}mqtt_msg_data_t;


static DBusHandlerResult dbus_msg_handle(DBusConnection *c, DBusMessage *m, void *data);
static FilterFuncsCallback filter_handle = {
            .message_handler = dbus_msg_handle,
            .free_func_handler = NULL,
            .data = NULL};



static int disc_finished = 0;
static int subscribed = 0;
static int finished = 0;

unsigned int get_timestamp(void)
{
    struct timeval stamp;
    gettimeofday(&stamp,NULL);
    return stamp.tv_sec;
}

static void cfinish(int sig)
{
    signal(SIGINT, NULL);
    printf("sig %d got\n", sig);
    //disc_finished = 1;
}

static DBusHandlerResult dbus_msg_handle(DBusConnection *c, DBusMessage *m, void *data)
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
    dbg(LOG_DEBUG, "if:%s, method:%s, obj:%s, type:%d", iface_path, member_name, obj_path, type);
    if ((NULL == iface_path) || (NULL == member_name) || (NULL == obj_path))
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (0 == strcmp(iface_path, "iot.mqtt.method"))
    {
        if (0 == strcmp(member_name, "ch2o_warning"))
        {
            const char *body = "{\"deviceId\":\"%s\", \"success\":true, \"properties\":{\"ch2o_warning_data\":\"%s\"}}";
            get_single_string_from_message(m, &param);
            if (NULL != param)
            {
                dbg(LOG_DEBUG, "recv ch2o value is %s", param);
                snprintf(buf, sizeof(buf), body, CLIENTID, param);
                mqtt_pubmsg(client, "/report-property", buf);
            }

        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

int gpio_set(DBusConnection *conn, int pin, int on_off)
{
    IpcPath dst_path_set = {
            .bus_path = "iot.device.gpio",
            .obj_path = "/iot/object/method",
            .interface_path = "iot.gpio.method",
            .act.method_name = "SetGpioStatus"
        };

    const char *gpio_status[2] = {"0", "1"};
    char *gpio_value_now = NULL;

    return send_method_call_without_reply(conn, &dst_path_set, gpio_status[on_off]);
}

static int gpio_status_publish(DBusConnection *conn, void *context, int pin, const char *topic, MQTTAsync_message *message)
{
    MQTTAsync *client = (MQTTAsync *)context;
    const char *payload;
    int on_off, rc;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    char *gpio_value = NULL;
    cJSON *json = NULL;
    char *messageId = NULL;
    char buf[256] = {0};
    unsigned long stamp;

    IpcPath dst_path_set = {
            .bus_path = "iot.device.gpio",
            .obj_path = "/iot/object/method",
            .interface_path = "iot.gpio.method",
            .act.method_name = "GetGpioStatus"
        };
    send_method_call_with_reply(conn, &dst_path_set, NULL, &gpio_value);
    dbg(LOG_DEBUG, "gpio value is %s", gpio_value);
    if (NULL == message) // 属性上报
    {
        const char *body = "{\"messageId\":\"%s\", \"properties\":[\"leds_status\":%s]}";
        snprintf(buf, sizeof(buf), body, "1", gpio_value);
    }
    else // 属性回复
    {
        const char *body = "{\"messageId\":\"%s\", \"deviceId\":\"%s\", \"success\":true, \"properties\":{\"leds_status\":\"%s\"}}";
        json = cJSON_Parse(message->payload);
        messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
        dbg(LOG_DEBUG, "message id %s", messageId);
        //stamp = get_timestamp();
        snprintf(buf, sizeof(buf), body, messageId, CLIENTID, gpio_value);
        
    }
    mqtt_pubmsg(client, topic, buf);
    
    if (json)
    {
        cJSON_Delete(json);
    }
    return rc;
}

static int ze08_status_publish(DBusConnection *conn, MQTTAsync *client, const char *topic, MQTTAsync_message *message)
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
            .bus_path = "iot.device.ze08",
            .obj_path = "/iot/object/method",
            .interface_path = "iot.ze08.method",
            .act.method_name = "read"
        };
    send_method_call_with_reply(conn, &dst_path_set, NULL, &ch2o_value);
    dbg(LOG_DEBUG, "ch2o value is %s", ch2o_value);
    json = cJSON_Parse(message->payload);
    messageId = cJSON_GetObjectItem(json, "messageId")->valuestring;
    dbg(LOG_DEBUG, "message id %s", messageId);
    snprintf(buf, sizeof(buf), body, messageId, CLIENTID, ch2o_value);
    mqtt_pubmsg(client, topic, buf);
    
    if (json)
    {
        cJSON_Delete(json);
    }
    return rc;
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
	dbg(LOG_WARNING, "Subscribe failed, rc %d\n", response ? response->code : 0);
}

void onconnect(void* context, MQTTAsync_successData* response)
{
#if 0
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;
#endif /* 0 */

	dbg(LOG_DEBUG, "Successful connection");

#if 0
	dbg(LOG_DEBUG, "Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n", TOPIC, CLIENTID, QOS);
	opts.onSuccess = onsubscribe;
	opts.onFailure = onsubscribeFailure;
	opts.context = client;

	if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{
		dbg(LOG_ERROR, "Failed to start subscribe, return code %d", rc);
	}
#endif /* 0 */
}

int mqtt_pubmsg(MQTTAsync *client, const char *topic, const char *payload)
{
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    int rc;

    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    dbg(LOG_DEBUG, "reply message %s", payload);
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, NULL)) != MQTTASYNC_SUCCESS)
    {
        dbg(LOG_WARNING, "Failed to publish message, return code %d\n", rc);
    }    
    return rc;
}

static void mqtt_msg_dispatch(DBusConnection *dbus_conn, MQTTAsync *client, char *topicName, MQTTAsync_message *message)
{
    cJSON *json, *json_properties, *json_properties_iter, *json_args;
    int properties_size;
    int i;
    char *properties_name, *function_name, *messageId, *deviceId;
    char buf[256] = {0};
    
    json = cJSON_Parse(message->payload);
    // {"args":[{"name":"off","value":"0"}],"function":"turn_off_led","messageId":"1261185185820741632","deviceId":"gateway-1"}
    /**

    messageId: message.messageId,        
    deviceId: message.deviceId,        
    timestamp: new Date().getTime(),        
    output: "ok", //返回结果        
    success: true

    */
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
        deviceId = cJSON_GetObjectItem(json, "deviceId")->valuestring;
        snprintf(buf, sizeof(buf), body, messageId, deviceId);
        mqtt_pubmsg(client, topicName, buf);
        
    }
    else if (0 == strcmp(topicName, "/read-property"))
    {
        json_properties = cJSON_GetObjectItem(json, "properties");
        dbg(LOG_DEBUG, "properties type is %d", json_properties->type);
        properties_size = cJSON_GetArraySize(json_properties);
        for (i=0; i<properties_size; i++)
        {
            json_properties_iter = cJSON_GetArrayItem(json_properties, i);
            properties_name = json_properties_iter->valuestring;
            dbg(LOG_DEBUG, "property name %s", properties_name);
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
    else
    {
        dbg(LOG_WARNING, "no match topic to be processed");
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

    dbg(LOG_DEBUG, "Message arrived");
    dbg(LOG_DEBUG, "     topic: %s", topicName);
    dbg(LOG_DEBUG, "   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');

    mqtt_msg_dispatch(the_conn, client, topicName, message);

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

	dbg(LOG_DEBUG, "Connection lost");
	if (cause)
		dbg(LOG_DEBUG, "     cause: %s", cause);

	dbg(LOG_DEBUG, "Reconnecting");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		dbg(LOG_WARNING, "Failed to start connect, return code %d", rc);
		finished = 1;
	}
}

void onconnectFailure(void* context, MQTTAsync_failureData* response)
{
	dbg(LOG_WARNING, "Connect failed, rc %d (%s)", response ? response->code : 0, response ? response->message : NULL);
	finished = 1;
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	dbg(LOG_WARNING, "Successful disconnection\n");
	disc_finished = 1;
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

    signal(SIGINT, cfinish);

    the_conn = connect_dbus(MQTT_AGENT_BUS_NAME, &rc);
    if (NULL == the_conn)
    {
        dbg(LOG_ERROR, "connect_dbus fail, rc = %d", rc);
        return 1;
    }

    poll_data = register_watch_functions_default(the_conn);
    if (NULL == poll_data)
    {
        dbg(LOG_ERROR, "Failed to register watch handler callback");
        return 1;
    }


    ret = MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != 0)
    {
        dbg(LOG_ERROR, "MQTTAsync_create return %d\n", ret);
        dbus_connection_unref(the_conn);
        return 1;
    }

    msg_data_context.client = client;
    msg_data_context.conn = the_conn;
    
    rc = MQTTAsync_setCallbacks(client, &msg_data_context, connlost, msgarrvd, NULL);
    if (ret != 0)
        printf("MQTTAsync_setCallbacks return %d\n", ret);

    conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = "led1";
	conn_opts.password = "led1";
	conn_opts.onSuccess = onconnect;
	conn_opts.onFailure = onconnectFailure;
	conn_opts.context = &msg_data_context;

	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

	filter_handle.data = client;

	ret = register_filter_functions(the_conn, &filter_handle);
    if (ret == FALSE)
    {
        dbg(LOG_ERROR, "Failed to register msg handler callback");
        return 1;
    }

    loop_start(the_conn, (void *)poll_data);

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
    MQTTAsync_destroy(&client);
    return rc;
}


