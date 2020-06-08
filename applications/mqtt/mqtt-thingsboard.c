#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "MQTTAsync.h"
#include <unistd.h>
#include "cJSON.h"
#include "dbus-base.h"
#include "logger.h"


#define ADDRESS     "tcp://192.168.1.188:1883"
#define CLIENTID    "BeagleBone-1"
#define TOPIC       "v1/devices/me/rpc/request/+"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TOPIC_RESONSE  "v1/devices/me/rpc/response/"
#define GPIO_STATUS_TOPIC "v1/devices/me/attributes"

#define MQTT_AGENT_BUS_NAME "iot.device.mqtt"

static DBusHandlerResult dbus_msg_handle(DBusConnection *c, DBusMessage *m, void *data);


static FilterFuncsCallback filter_handle = {
            .message_handler = dbus_msg_handle,
            .free_func_handler = NULL,
            .data = NULL};

static DBusConnection *the_conn;

static int disc_finished = 0;
static int subscribed = 0;
static int finished = 0;

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

    iface_path = dbus_message_get_interface(m);
    member_name = dbus_message_get_member(m);
    obj_path = dbus_message_get_path(m);
    type = dbus_message_get_type(m);
    dbg(LOG_DEBUG, "if:%s, method:%s, obj:%s, type:%d", iface_path, member_name, obj_path, type);
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

    send_method_call_without_reply(conn, &dst_path_set, gpio_status[on_off]);
}

int gpio_status_publish(DBusConnection *conn, void *context, int pin, const char *topic)
{
    MQTTAsync *client = (MQTTAsync *)context;
    const char *gpio_status[3] = {"{\"7\":false}", "{\"7\":true}", "{\"7\":invalid}"};
    const char *payload;
    int on_off, rc;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    char *gpio_value = NULL;
    IpcPath dst_path_set = {
            .bus_path = "iot.device.gpio",
            .obj_path = "/iot/object/method",
            .interface_path = "iot.gpio.method",
            .act.method_name = "GetGpioStatus"
        };
    send_method_call_with_reply(conn, &dst_path_set, NULL, &gpio_value);
    dbg(LOG_DEBUG, "gpio value is %s", gpio_value);
    if (NULL == gpio_value)
    {
        payload = gpio_status[2];
    }
    else
    {
        on_off = atoi(gpio_value);
        payload = (on_off == 0) ? gpio_status[0] : gpio_status[1];
    }
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to gpio_status_publish sendMessage, return code %d\n", rc);
    }
    return rc;
}


void onsubscribe(void* context, MQTTAsync_successData* response)
{
    MQTTAsync *client = (MQTTAsync *)context;
	
	subscribed = 1;

    /** 订阅成功后，向服务端publish状态信息 */
	gpio_status_publish(the_conn, client, 53, GPIO_STATUS_TOPIC);
}

void onsubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	dbg(LOG_WARNING, "Subscribe failed, rc %d\n", response ? response->code : 0);
}

void onconnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;

	dbg(LOG_DEBUG, "Successful connection");

	dbg(LOG_DEBUG, "Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n", TOPIC, CLIENTID, QOS);
	opts.onSuccess = onsubscribe;
	opts.onFailure = onsubscribeFailure;
	opts.context = client;

	if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{
		dbg(LOG_ERROR, "Failed to start subscribe, return code %d", rc);
	}
}

/**
 * 
 * @brief 因为topic形式如：v1/devices/me/rpc/request/93,此函数用来提取末尾的数字
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
int get_topic_sequence(const char *topic)
{
    char *num_start = strrchr(topic, '/');
    return atoi(num_start+1);
    
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i, rc;
    char* payloadptr;
    cJSON *json, *method_json, *params_json;
    MQTTAsync *client = (MQTTAsync *)context;
    int pin, on_off, sequence;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    char res_buf[32] = {0};

    dbg(LOG_DEBUG, "Message arrived");
    dbg(LOG_DEBUG, "     topic: %s", topicName);
    dbg(LOG_DEBUG, "   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');

    json = cJSON_Parse(message->payload);
    method_json = cJSON_GetObjectItem(json, "method");
    sequence = get_topic_sequence(topicName);
    dbg(LOG_DEBUG, "response seq = %d\n", sequence);
    snprintf(res_buf, sizeof(res_buf), "%s%d", TOPIC_RESONSE, sequence);
    if (0 == strcmp(method_json->valuestring, "setGpioStatus"))
    {
        params_json = cJSON_GetObjectItem(json, "params");
        pin = cJSON_GetObjectItem(params_json, "pin")->valuedouble;
        on_off = cJSON_GetObjectItem(params_json, "enabled")->valueint;
        dbg(LOG_DEBUG, "pin no. is %d, set status = %d\n", pin, on_off);
        gpio_set(the_conn, 53, on_off);
    	gpio_status_publish(the_conn, client, 53, res_buf);
        gpio_status_publish(the_conn, client, 53, GPIO_STATUS_TOPIC);
        
    }
    else if (0 == strcmp(method_json->valuestring, "getGpioStatus"))
    {
        gpio_status_publish(the_conn, client, 53, res_buf);
    }
    
    cJSON_Delete(json);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
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
	dbg(LOG_WARNING, "Connect failed, rc %d", response ? response->code : 0);
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
    ret = register_filter_functions(the_conn, &filter_handle);
    if (ret == FALSE)
    {
        dbg(LOG_ERROR, "Failed to register msg handler callback");
        return 1;
    }

    ret = MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != 0)
        printf("MQTTAsync_create return %d\n", ret);
    MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL);
    if (ret != 0)
        printf("MQTTAsync_setCallbacks return %d\n", ret);

    conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = "RASPBERRY_PI_DEMO_TOKEN";
	conn_opts.onSuccess = onconnect;
	conn_opts.onFailure = onconnectFailure;
	conn_opts.context = client;

	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

    loop_start(the_conn, (void *)poll_data);

    disc_opts.onSuccess = onDisconnect;
    if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start disconnect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    
    while (!disc_finished)
    {
        usleep(10000L);
    }

exit:
    MQTTAsync_destroy(&client);
    return rc;
}


