#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pthread.h>
#include "stdint.h"
#include "dbus-base.h"
#include "logger.h"
#include "dbus_ipc_name.h"
#include "parse-config.h"
#define SHTC1_CONFIG_FILE "/mnt/shtc1.json"


static DBusHandlerResult shtc1_filter_func(DBusConnection *c, DBusMessage *m, void *data);

static FilterFuncsCallback shtc1_msg_handler = {
            .message_handler = shtc1_filter_func,
            .free_func_handler = NULL,
            .data = NULL};


static int loop = 1;
static int shtc1_process_flag = 1;
static double g_temp_high_threshold = 35;
static double g_temp_low_threshold = 4;
static cJSON* json = NULL;
typedef struct
{
    double high;
    double low;
}temp_threshold;
#if 0
static double get_shtc1_config(const char *string_name)
{
    cJSON* c = cJSON_GetObjectItem(json, string_name);
    double result = c->valuedouble;
    return result;
}
#endif /* 0 */
static int get_shtc1_config(void *data, int argc, char **argv, char **azColName)
{
    int i = 0;
    temp_threshold *threshold = (temp_threshold *)data;
    for (i = 0; i < argc; i++) 
    {
        if (0 == strcmp(azColName[i], "high_threshold"))
        {
            threshold->high = atof(argv[i]);
        }
        else if (0 == strcmp(azColName[i], "low_threshold"))
        {
            threshold->low = atof(argv[i]);
        }
    }
    return 0;
}
static void update_threshold(const char* name, const double value)
{
    char sql[128] = {0};
    const char *sql_template = "update shtc1 set %s = %f where id = 1;";
    sprintf(sql, sql_template, name, value);
    op_sqlite_db(DB_PATH, sql, NULL, NULL);
}

static int get_result_from_shtc1(const char *path)
{
    int fd;
    char temp[8] = {0};
    int real_temp = 0;
    fd = open(path, O_RDONLY);
    if (fd)
    {
        read(fd, temp, 8);
        real_temp = atoi(temp);
        close(fd);
        //return (real_temp & 0xffff);
        return real_temp;
    }
    else
    {
        dbg(IOT_WARNING, "%s open fail", path);
        return 0;
    }
}

static DBusHandlerResult shtc1_filter_func(DBusConnection *c, DBusMessage *m, void *data)
{
    const char *iface_path, *member_name;
    float temp, humidity = 0;
    char temp_f[8] = {0};
    char humidity_f[8] = {0};
    char *p_temp = temp_f;
    char *p_humidity = humidity_f;
    char *p_temp_threshold = NULL;
    iface_path = dbus_message_get_interface(m);
    member_name = dbus_message_get_member(m);
    dbg(IOT_DEBUG, "if:%s, method:%s", iface_path, member_name);
    if ((NULL == iface_path) || (NULL == member_name))
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (0 == strcmp(iface_path, SHTC1_IFACE_PATH))
    {
        if (0 == strcmp(member_name, SHTC1_GET_METHOD_TEMP))
        {
            temp = (float)get_result_from_shtc1("/sys/class/hwmon/hwmon0/temp1_input") / 1000;
            snprintf(temp_f, 8, "%.1f", temp);
            if (FALSE == send_method_return(c, m, DBUS_TYPE_STRING, &p_temp))
            {
                dbg(IOT_WARNING, "send method fail: [%s] to be sent", temp_f);
            }
        }
        else if (0 == strcmp(member_name, SHTC1_GET_METHOD_TEMP_THRESHOLD_HIGH))
        {
            snprintf(temp_f, 8, "%.1f", g_temp_high_threshold);
            if (FALSE == send_method_return(c, m, DBUS_TYPE_STRING, &p_temp))
            {
                dbg(IOT_WARNING, "send method fail: [%s] to be sent", temp_f);
            }
        }
        else if (0 == strcmp(member_name, SHTC1_GET_METHOD_TEMP_THRESHOLD_LOW))
        {
            snprintf(temp_f, 8, "%.1f", g_temp_low_threshold);
            if (FALSE == send_method_return(c, m, DBUS_TYPE_STRING, &p_temp))
            {
                dbg(IOT_WARNING, "send method fail: [%s] to be sent", temp_f);
            }
        }
        else if (0 == strcmp(member_name, SHTC1_GET_METHOD_HUMIDITY))
        {
            humidity = (float)get_result_from_shtc1("/sys/class/hwmon/hwmon0/humidity1_input") / 1000;
            snprintf(humidity_f, 8, "%.1f", humidity);
            if (FALSE == send_method_return(c, m, DBUS_TYPE_STRING, &p_humidity))
            {
                dbg(IOT_WARNING, "send method fail: [%s] to be sent", humidity_f);
            }
        }
        else if (0 == strcmp(member_name, SHTC1_SET_TEMP_HITH_THRESHOLD))
        {
            get_single_arg_from_message(m, (void*)&p_temp_threshold);
            dbg(IOT_DEBUG, "temp_high_threshold = %s", p_temp_threshold);
            g_temp_high_threshold = atof(p_temp_threshold);
            update_threshold("high_threshold", g_temp_high_threshold);
        }
        else if (0 == strcmp(member_name, SHTC1_SET_TEMP_LOW_THRESHOLD))
        {
            get_single_arg_from_message(m, (void*)&p_temp_threshold);
            dbg(IOT_DEBUG, "temp_high_threshold = %s", p_temp_threshold);
            g_temp_low_threshold = atof(p_temp_threshold);
            update_threshold("low_threshold", g_temp_low_threshold);
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;

}

static void cfinish(int sig)
{	
    signal(SIGINT, NULL);
    printf("finished\n");
    loop = 0;
}

void *shtc1_process(void* args)
{
    float temp1 = 0, humidity1 = 0;
    DBusConnection *conn = (DBusConnection *)args;
    int high_flag_old = 0, high_flag_new = 0, low_flag_old = 0, low_flag_new = 0;
    IpcPath emit_path_set_hith = {
        .bus_path = SHTC1_BUS_NAME,
        .obj_path = SHTC1_OBJ_PATH,
        .interface_path = SHTC1_IFACE_PATH,
        .act.signal_name = "temp_over"
    };
    IpcPath emit_path_set_low = {
        .bus_path = SHTC1_BUS_NAME,
        .obj_path = SHTC1_OBJ_PATH,
        .interface_path = SHTC1_IFACE_PATH,
        .act.signal_name = "temp_below"
    };
    char temp1_buf[8] = {0};
    char *p_temp1_buf = temp1_buf;
#if 0
    json = get_file_to_json(SHTC1_CONFIG_FILE);
#endif /* 0 */
    temp_threshold threshold;
    const char *sql = "select * from shtc1 where id = 1;";
    op_sqlite_db(DB_PATH, sql, get_shtc1_config, (void *)&threshold);
    g_temp_high_threshold = threshold.high;
    g_temp_low_threshold = threshold.low;
    while (loop)
    {
        temp1 = (float)get_result_from_shtc1("/sys/class/hwmon/hwmon0/temp1_input") / 1000;
        humidity1 = (float)get_result_from_shtc1("/sys/class/hwmon/hwmon0/humidity1_input") / 1000;

        if (temp1 > g_temp_high_threshold)
        {
            high_flag_new = 1;
        }
        else
        {
            high_flag_new = 0;
        }

        if (high_flag_old ^ high_flag_new)
        {
            if (high_flag_new)
            {
            snprintf(temp1_buf, 8, "%.1f", temp1);
                send_signal(conn, &emit_path_set_hith, DBUS_TYPE_STRING, &p_temp1_buf);
            }
            high_flag_old = high_flag_new;
        }
        
        if (temp1 < g_temp_low_threshold)
        {
            low_flag_new = 1;
        }
        else
        {
            low_flag_new = 0;
        }

        if (low_flag_old ^ low_flag_new)
        {
            if (low_flag_new)
            {
                snprintf(temp1_buf, 8, "%.1f", temp1);
                send_signal(conn, &emit_path_set_low, DBUS_TYPE_STRING, &p_temp1_buf);
            }
            low_flag_old = low_flag_new;
        }
        
        sleep(1);
    }
}

int main(int argc, char** argv) 
{	

    DBusConnection *conn;
    int32_t rc = 0;
    FdWatchMap *poll_data;
    dbus_bool_t ret = FALSE;
    pthread_t ntid, ptid;
   
    conn = connect_dbus(SHTC1_BUS_NAME, &rc);
    if (NULL == conn)
    {
        dbg(IOT_ERROR, "connect_dbus fail, rc = %d", rc);
        exit(1);
    }

    poll_data = register_watch_functions_default(conn);
    if (NULL == poll_data)
    {
        printf("Failed to register watch handler callback\n");
        exit(1);
    }
    ret = register_filter_functions(conn, &shtc1_msg_handler);
    if (ret == FALSE)
    {
        printf("Failed to register msg handler callback\n");
        exit(1);
    }

    signal(SIGINT, cfinish);

    rc = pthread_create(&ntid, NULL, shtc1_process, (void *)conn);
    if (rc != 0)
    {
        dbg(IOT_ERROR, "pthread create fail");
        exit(1);
    }
    
    dbus_loop_start(conn, (void *)poll_data);

    while (shtc1_process_flag)
    {
        sleep(1);
    }

    return 0;
    
}


