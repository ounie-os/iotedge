#include "dbus-base.h"
#include "gpio-api.h"
#include "logger.h"
#include "signal.h"
#include "dbus_ipc_name.h"

static DBusHandlerResult filter_func_default(DBusConnection *conn, DBusMessage *msg, void *data);
static void gpio_flash_status(int gpio, int on_ff);

static FilterFuncsCallback ffc = {
            .message_handler = filter_func_default,
            .free_func_handler = NULL,
            .data = NULL};

static DBusHandlerResult filter_func_default(DBusConnection *c, DBusMessage *m, void *data)
{
    DBusMessage *reply;
    DBusMessageIter arg;
    char *param = NULL;
    const char *iface_path, *member_name;
    char gpio_status[8] = {0};
    char *p_gpio_status;

    iface_path = dbus_message_get_interface(m);
    member_name = dbus_message_get_member(m);
    dbg(IOT_DEBUG, "if:%s, method:%s", iface_path, member_name);

    if (0 == strcmp(iface_path, GPIO_IFACE_PATH))
    {
        if (0 == strcmp(member_name, GPIO_SET_METHOD_NAME))
        {
            get_single_arg_from_message(m, &param);
            if (NULL != param)
            {
                if (0 == strcmp(param, "0"))
                {
                    gpio_put_status(53, 0);
                }
                else if (0 == strcmp(param, "1"))
                {
                    gpio_put_status(53, 1);
                }
                else
                {
                    dbg(IOT_WARNING, "%s param is %s", member_name, param);
                }
            }
            else
            {
                dbg(IOT_WARNING, "param is null");
            }
        }
        else if (0 == strcmp(member_name, GPIO_FLASH_SET_METHOD_NAME))
        {  
            get_single_arg_from_message(m, &param);
                if (NULL != param)
                {
                    if (0 == strcmp(param, "0"))
                    {
                        gpio_flash_status(53, 0);
                    }
                    else if (0 == strcmp(param, "1"))
                    {
                        gpio_flash_status(53, 1);
                    }
                    else
                    {
                        dbg(IOT_WARNING, "%s param is %s", member_name, param);
                    }
                }
                else
                {
                    dbg(IOT_WARNING, "param is null");
                }
        }

        snprintf(gpio_status, sizeof(gpio_status), "%d", gpio_get_status(53));
        p_gpio_status = &gpio_status[0];
        if (FALSE == send_method_return(c, m, DBUS_TYPE_STRING, &p_gpio_status))
        {
            dbg(IOT_WARNING, "send method fail: [%s] to be sent", p_gpio_status);
        }
        dbg(IOT_DEBUG, "send method ok: [%s] sent", p_gpio_status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static void cfinish(int sig)
{	
    signal(SIGINT, NULL);
    gpio_free(53);
    dbg(IOT_DEBUG, "got signal %d", sig);
    exit(0);
}

static void gpio_flash_thread(void *arg)
{
    int p_gpio = *(int *)arg;
    while (1) 
    {
        usleep(500000);
        gpio_put_status(53, 1);
        usleep(500000);
        gpio_put_status(53, 0);
    }
}

static pthread_t gpio_flash_tid;

static void gpio_flash_status(int gpio, int on_ff)
{
    int rc = 0;
    
    if (on_ff) 
    {
        rc = pthread_create(&gpio_flash_tid, NULL, gpio_flash_thread, (void*)&gpio);
        if (rc != 0)
        {
            dbg(IOT_ERROR, "pthread create fail");
        }
    }
    else
    {
        pthread_cancel(gpio_flash_tid);
        gpio_put_status(53, 0);
    }
}

int main(int argc, char *argv[])
{
    DBusConnection *conn;
    FdWatchMap *poll_data;
    int32_t rc = 0;
    dbus_bool_t ret = FALSE;
    conn = connect_dbus(GPIO_BUS_NAME, &rc);
    if (NULL == conn)
    {
        dbg(IOT_ERROR, "connect_dbus fail, rc = %d", rc);
        exit(1);
    }

    if ( -1 != gpio_request(53))
	{
		if ( -1 != gpio_set_direction(53, 1))
		{
            dbg(IOT_DEBUG, "USR0 GPIO is on");
		}
	}
	signal(SIGINT, cfinish);
#if 0

    //memset(&fd_watch_map, 0, sizeof(fd_watch_map));
    //ret = register_watch_functions(conn, &wfb);
    if (ret == FALSE)
    {
        printf("dbus_connection_set_watch_functions failed\n");
        exit(1);
    }
#endif /* 0 */

    poll_data = register_watch_functions_default(conn);
    if (NULL == poll_data)
    {
        printf("Failed to register watch handler callback\n");
        exit(1);
    }
    ret = register_filter_functions(conn, &ffc);
    if (ret == FALSE)
    {
        printf("Failed to register msg handler callback\n");
        exit(1);
    }
    register_timeout_functions(conn, "gpio time out func");
    

    dbus_loop_start(conn, (void *)poll_data);
    
}