#include <coap2/coap.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "dbus-base.h"
#include "logger.h"
#include "dbus_ipc_name.h"


#ifdef __GNUC__
#define UNUSED_PARAM __attribute__ ((unused))
#else /* not a GCC */
#define UNUSED_PARAM
#endif /* GCC */

static uint32_t coap_process_loop = 1;


typedef struct
{
    char value[32];
    pthread_mutex_t lock;
}ze08_ppm_t;

static ze08_ppm_t coap_ze08_value;

static DBusHandlerResult coap_filter_func(DBusConnection *c, DBusMessage *m, void *data);

static FilterFuncsCallback coap_msg_handler = {
            .message_handler = coap_filter_func,
            .free_func_handler = NULL,
            .data = NULL};

static DBusHandlerResult coap_filter_func(DBusConnection *c, DBusMessage *m, void *data)
{
    const char *iface_path, *member_name;
    int32_t message_type = 0;
    uint32_t ppm = 0;
    char *ppm_str = NULL;
    iface_path = dbus_message_get_interface(m);
    member_name = dbus_message_get_member(m);
    message_type = dbus_message_get_type(m);
    dbg(IOT_DEBUG, "if:%s, method:%s, type:%d", iface_path, member_name, message_type);
    if ((NULL == iface_path) || (NULL == member_name))
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (message_type == DBUS_MESSAGE_TYPE_SIGNAL && (0 == strcmp(iface_path, ZE08_IFACE_PATH)))
    {
        if (0 == strcmp(member_name, "ch2o_warning_float"))
        {

            get_single_arg_from_message(m, &ppm);
            dbg(IOT_DEBUG, "param = 0x%x", ppm);
        }
        else if (0 == strcmp(member_name, "ch2o_warning"))
        {
            get_single_arg_from_message(m, &ppm_str);
            dbg(IOT_DEBUG, "str = %s", ppm_str);
            pthread_mutex_lock(&coap_ze08_value.lock);
            strncpy(coap_ze08_value.value, ppm_str, 15);
            coap_ze08_value.value[15] = '\0';
            pthread_mutex_unlock(&coap_ze08_value.lock);          
        }
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}


static int resolve_address(const char *host, const char *service, coap_address_t *dst) 
{  
    struct addrinfo *res, *ainfo;  
    struct addrinfo hints;  
    int error, len=-1;  
    memset(&hints, 0, sizeof(hints));  
    memset(dst, 0, sizeof(*dst));  
    hints.ai_socktype = SOCK_DGRAM;  
    hints.ai_family = AF_UNSPEC;  
    error = getaddrinfo(host, service, &hints, &res);  
    if (error != 0) 
    {    
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));    return error;  
    }  

    for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next) 
    {    
        switch (ainfo->ai_family) 
        {    
            case AF_INET6:    
            case AF_INET:      
                len = dst->size = ainfo->ai_addrlen;      
                memcpy(&dst->addr.sin6, ainfo->ai_addr, dst->size);      
                goto finish;    
            default:      
                ;    
        }  
    } 
    finish:  
    freeaddrinfo(res);  
    return len;
}


static void hello_get_handler(coap_context_t  *ctx UNUSED_PARAM,
                               struct coap_resource_t *resource,
                               coap_session_t *session,
                               coap_pdu_t *request,
                               coap_binary_t *token /* token */,
                               coap_string_t *query UNUSED_PARAM /* query string */,
                               coap_pdu_t *response /* response */)
{
    uint8_t buf[32] = {0};
    int32_t len = 0;
    pthread_mutex_lock(&coap_ze08_value.lock);
    len = snprintf(buf, sizeof(buf)-1, "{ze08:%s}", coap_ze08_value.value);
    dbg(IOT_DEBUG, "ze08 value len = %d(%s)", len, &coap_ze08_value.value[0]);
    pthread_mutex_unlock(&coap_ze08_value.lock);
    coap_add_data_blocked_response(resource, session, request, response, token, COAP_MEDIATYPE_APPLICATION_JSON, 0, len, (const uint8_t *)buf);
}

static void cfinish(int sig)
{
    signal(SIGINT, NULL);
    printf("sig %d got\n", sig);
    coap_process_loop = 0;
}

void *coap_process(void *arg)
{
    coap_context_t *ctx = (coap_context_t *)arg;
    while (coap_process_loop)
    {
        coap_run_once(ctx, 0);
    }
    dbg(IOT_DEBUG, "thread finished");
}


int main(int argc, char **argv)
{
    coap_context_t *ctx = NULL;
    coap_address_t dst;
    coap_resource_t *resource = NULL;
    coap_endpoint_t *endpoint = NULL;
    DBusConnection *conn;
    FdWatchMap *poll_data;
    dbus_bool_t ret;
    int32_t rc = 0;
    int32_t result = EXIT_FAILURE;
    pthread_t ntid;
    coap_str_const_t *ruri = coap_make_str_const("hello");

    if (resolve_address("192.168.1.250", "5683", &dst) < 0)
    {
        dbg(IOT_WARNING, "failed to resolve address\n");
        goto finish;
    }

    ctx = coap_new_context(NULL);

    if (!ctx || !(endpoint = coap_new_endpoint(ctx, &dst, COAP_PROTO_UDP))) 
    {    
        dbg(IOT_ERROR, "cannot initialize context\n");    
        goto finish;  
    }

    conn = connect_dbus(COAP_BUS_NAME, &rc);
    if (NULL == conn)
    {
        dbg(IOT_ERROR, "connect_dbus fail, rc = %d", rc);
        goto finish;
    }

    poll_data = register_watch_functions_default(conn);
    if (NULL == poll_data)
    {
        printf("Failed to register watch handler callback\n");
        goto finish;
    }
    ret = register_filter_functions(conn, &coap_msg_handler);
    if (ret == FALSE)
    {
        printf("Failed to register msg handler callback\n");
        goto finish;
    }

    dbus_add_match(conn, "type='signal', interface="ZE08_IFACE_PATH);

    signal(SIGINT, cfinish);

    resource = coap_resource_init(ruri, 0);
    coap_register_handler(resource, COAP_REQUEST_GET, hello_get_handler);
    coap_add_resource(ctx, resource);

    pthread_mutex_init(&coap_ze08_value.lock, NULL);
    memset(coap_ze08_value.value, 0, 16);

    rc = pthread_create(&ntid, NULL, coap_process, (void *)ctx);
    if (rc != 0)
    {
        dbg(IOT_ERROR, "pthread create fail");
        goto finish;
    }
    
    dbus_loop_start(conn, (void *)poll_data);

    result = EXIT_SUCCESS;
    
    finish:  
    coap_free_context(ctx);  
    coap_cleanup();  
    return result;
}
