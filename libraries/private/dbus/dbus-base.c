#include "dbus-base.h"
#include "logger.h"

static FdWatchMap _fd_poll;

dbus_bool_t add_timeout_default(DBusTimeout *timeout, void *data)
{
    dbg(IOT_DEBUG, "data is %s", data);
    return 1;
}

dbus_bool_t register_timeout_functions(DBusConnection *connection, void *data)
{
    dbus_bool_t rc;
    dbg(IOT_DEBUG, " ");
    //DBusTimeout *timeout_handle = _dbus_timeout_new(1000, time_handle, data, NULL);
    //_dbus_connection_add_timeout_unlocked(connection, timeout_handle);
    rc = dbus_connection_set_timeout_functions(connection, add_timeout_default, NULL, NULL, data, NULL);
    if (FALSE == rc)
    {
        dbg(IOT_ERROR, "dbus_connection_set_timeout_functions fail");
        
    }
    //_dbus_timeout_unref(timeout_handle);
    return rc;
}

#if 0
void add_timeout_function(DBusConnection *connection, TimeOutItem *toi, TimeOutCallback *tocb)
{
    dbus_bool_t rc;
    DBusTimeout *timeout_handle = _dbus_timeout_new(toi->interval, toi->handler, toi->data, toi->free_data_function);
    rc = dbus_connection_set_timeout_functions(connection, tocb->add_timeout_handle, tocb->remove_timeout_handle, 
                                            tocb->toggle_timeout_handle, tocb->data, tocb->free_func_handler);
}
#endif /* 0 */

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
void get_single_arg_from_message(DBusMessage* m, void *param)
{
    DBusMessageIter arg;
    
    if (FALSE == dbus_message_iter_init(m, &arg))
    {
        dbg(IOT_ERROR, "there is no arg in the message");
    }
    else
    {
        dbus_message_iter_get_basic(&arg, param);
    }
}

/**
 * 
 * @brief 给接收到的消息发送对应的回复
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
dbus_bool_t send_method_return(DBusConnection *connection, DBusMessage *m, int dbus_type, const char *str)
{
    DBusMessage *reply;
    DBusMessageIter arg;
    reply = dbus_message_new_method_return(m);
    if (NULL == reply)
    {
        dbg(IOT_ERROR, "request method return msg fail");
        return FALSE;
    }
    dbus_message_iter_init_append(reply, &arg);

    if (FALSE == dbus_message_iter_append_basic(&arg, dbus_type, str))
    {
        dbg(IOT_ERROR, "out of memory");
        dbus_message_unref(reply);
        return FALSE;
    }
    
    if (FALSE == dbus_connection_send(connection, reply, NULL))
    {
        dbg(IOT_ERROR, "out of memory");
        dbus_message_unref(reply);
        return FALSE;
    }

    return TRUE;
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
dbus_bool_t send_signal(DBusConnection *connection, IpcPath *path_set, int dbus_type, const void *sigval)
{
    DBusMessage *msg;
    DBusMessageIter arg;

    if ((NULL == connection) || (NULL == path_set))
    {
        dbg(IOT_ERROR, "there is null incoming params\n");
        return FALSE;
    }
    
    const char *obj_path = path_set->obj_path;
    const char *iface_path = path_set->interface_path;
    const char *siganl_name = path_set->act.signal_name;

    if ((NULL == obj_path) || (NULL == iface_path) || (NULL == siganl_name))
    {
        dbg(IOT_ERROR, "check path_set member");
        return FALSE;
    }

    msg = dbus_message_new_signal(obj_path, iface_path, siganl_name);
    if (NULL == msg)
    {
        dbg(IOT_ERROR, "dbus_message_new_signal request fail");
    }

    if (sigval)
    {
        dbus_message_iter_init_append(msg, &arg);
        if (!dbus_message_iter_append_basic(&arg, dbus_type, sigval))
        {
            dbg(IOT_ERROR, "need more memory");
            dbus_message_unref(msg);
            return FALSE;
        }
    }
    
    if (!dbus_connection_send(connection, msg, NULL))
    {
        dbg(IOT_ERROR, "signal send fail");
        dbus_message_unref(msg);
        return FALSE;
    }

    dbus_message_unref(msg);

    return TRUE;
    
}

/**
 * 
 * @brief 只发送远程方法调用，不等待回复
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
dbus_bool_t send_method_call_without_reply(DBusConnection *connection, IpcPath *path_set, int dbus_type, const void *content)
{
    DBusMessage *msg;
    DBusMessageIter arg;

    const char *dst_bus = path_set->bus_path;
    const char *obj_path = path_set->obj_path;
    const char *iface_path = path_set->interface_path;
    const char *method_name = path_set->act.method_name;

    if ((NULL == connection) || (NULL == path_set))
    {
        dbg(IOT_ERROR, "there is null incoming params");
        return FALSE;
    }
    
    msg = dbus_message_new_method_call(dst_bus, obj_path, iface_path, method_name);
    if (msg == NULL){
        dbg(IOT_ERROR, "dbus_message_new_method_call request fail");
        return FALSE;
    }

    if (content)
    {
        dbus_message_iter_init_append(msg, &arg);
        if (!dbus_message_iter_append_basic(&arg, DBUS_TYPE_STRING, content)) 
        {
           dbg(IOT_ERROR, "Out of Memory!");
           dbus_message_unref(msg);
           return FALSE;
        }
    }

    if (!dbus_connection_send(connection, msg, NULL))
    {
        dbg(IOT_ERROR, "signal send fail");
        dbus_message_unref(msg);
        return FALSE;
    }

    dbus_message_unref(msg);

    return TRUE;
}

/**
 * 
 * @brief 发送远程方法调用并等待回应
 * 
 * @param[in]   @n connection 连接句柄
 * @param[in]   @n path_set 通信用路径集合
 * @param[in]   @n content 将要发送的消息，为NULL时表示方法调用中无具体发送信息
 * @param[out]  @n reply_content 方法调用后远端回复的消息内容
 * @return 
 */
dbus_bool_t send_method_call_with_reply(DBusConnection *connection, IpcPath *path_set, const char *content, char *reply_content)
{
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusPendingCall *pending;
#if 0
    const char *dst_bus = "test.wei.dest";
    const char *obj_path = "/test/method/Object";
    const char *interface_path = "test.method.Type";
    const char *method_name = "Method";
#endif /* 0 */
    if ((NULL == connection) || (NULL == path_set) || (NULL == reply_content))
    {
        dbg(IOT_ERROR, "there is null incoming params");
        return FALSE;
    }

    const char *dst_bus = path_set->bus_path;
    const char *obj_path = path_set->obj_path;
    const char *iface_path = path_set->interface_path;
    const char *method_name = path_set->act.method_name;

    /** 构建远程方法调用 
    * dst_bus 远程方法调用对应的目的bus名称，也即对方进程标识符
    * obj_path 
    */

    if ((NULL == dst_bus) || (NULL == obj_path) || (NULL == iface_path) || (NULL == method_name))
    {
        dbg(IOT_ERROR, "check path_set member");
        return FALSE;
    }
    
    msg = dbus_message_new_method_call(dst_bus, obj_path, iface_path, method_name);
    if (msg == NULL){
        dbg(IOT_ERROR, "dbus_message_new_method_call request fail");
        return FALSE;
    }

    if (content)
    {
        dbus_message_iter_init_append(msg, &arg);
        if (!dbus_message_iter_append_basic(&arg, DBUS_TYPE_STRING, content)) 
        {
           dbg(IOT_ERROR, "Out of Memory!");
           dbus_message_unref(msg);
           return FALSE;
        }
    }
    
    /** msg: 发送的消息 pending: 接收的消息 */
    if (!dbus_connection_send_with_reply (connection, msg, &pending, -1)) 
    {
       dbg(IOT_ERROR, "Out of Memory!");
       dbus_message_unref(msg);
       return FALSE;
    }     

    if (pending == NULL){
        dbg(IOT_ERROR, "Pending Call NULL: connection is disconnected");
        dbus_message_unref(msg);
        return FALSE;
    }

    dbus_connection_flush(connection);
    dbus_message_unref(msg);

    dbus_pending_call_block (pending);
    
    msg = dbus_pending_call_steal_reply (pending);
    if (msg == NULL) 
    {
        dbg(IOT_ERROR, "Reply Null");
        dbus_message_unref(msg);
        return FALSE;
    }

    dbus_pending_call_unref(pending);

    dbg(IOT_DEBUG, "return msg type is %d", dbus_message_get_type(msg));
    
    if (!dbus_message_iter_init(msg, &arg))
    {
        dbg(IOT_WARNING, "Message has no arguments!\n");
    }
    else
    {

#if 0
        if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            dbg(IOT_ERROR, "reply got, but arg type is not STRING");
            dbus_message_unref(msg);
            return FALSE;
        }
        else
#endif /* 0 */
        {
            dbus_message_iter_get_basic(&arg, reply_content);
        }
    }
    dbg(IOT_DEBUG, "Got Reply: %s\n", reply_content);
    
    dbus_message_unref(msg);
    return TRUE;
}


dbus_bool_t add_watch_default(DBusWatch *watch, void *data)
{
    short cond = POLLHUP | POLLERR;
    int fd;
    unsigned int flags;
    FdWatchMap *fd_watch = (FdWatchMap *)data;
    
    fd = dbus_watch_get_unix_fd(watch);
    flags = dbus_watch_get_flags(watch);
    dbg(IOT_DEBUG, "add watch %p, flags = 0x%x", (void*)watch, flags);
   
    if (flags & DBUS_WATCH_READABLE)
            cond |= POLLIN;
    if (flags & DBUS_WATCH_WRITABLE)
            cond |= POLLOUT;

    if (fd_watch->max_fd >= MAX_WATCHES)
    {
        printf("add watch reached max\n");
        return FALSE;
    }

    fd_watch->pollfds[fd_watch->max_fd].fd = fd;
    fd_watch->pollfds[fd_watch->max_fd].events = cond;
    fd_watch->watches[fd_watch->max_fd] = watch;
    fd_watch->max_fd++;
    return TRUE;
}

void remove_watch_default(DBusWatch *watch, void *data)
{
    int i, found = 0;
    FdWatchMap *fd_watch = (FdWatchMap *)data;
    dbg(IOT_DEBUG, "remove watch %p", (void*)watch);
    for (i = 0; i < fd_watch->max_fd; ++i) 
    {
        if (fd_watch->watches[i] == watch) 
        {
            found = 1;
            break;
        }
    }
    if (!found) {
        dbg(IOT_WARNING, "watch %p not found\n", (void*)watch);
        return;
    }
    memset(&fd_watch->pollfds[i], 0, sizeof(fd_watch->pollfds[i]));
    fd_watch->watches[i] = NULL;
    if (fd_watch->max_fd > 0) 
        fd_watch->max_fd--;
}

void toggle_watch_default(DBusWatch *watch, void *data)
{
    dbus_bool_t ret = FALSE;
    ret = dbus_watch_get_enabled(watch);
    if (FALSE == ret)
    {
        dbg(IOT_WARNING, "watch[%p] disabled", (void*)watch);
    }
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
DBusConnection *connect_dbus(const char *bus_name, int32_t *rc)
{
    DBusError err;
    DBusConnection *connection;
    int ret;

    dbus_error_init(&err);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (NULL == connection)
    {
        dbg(IOT_ERROR, "Connection Err:%s", err.message);
        dbus_error_free(&err);
        return NULL;
    }

    ret = dbus_bus_request_name(connection, bus_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err))
    {
        dbg(IOT_WARNING, "Bus Name Err:%s", err.message);
        dbus_error_free(&err);    
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
        if (NULL != rc)
        {
            *rc = ret;
        }
        dbg(IOT_WARNING, "ret is not DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER");
        return NULL;
    }
    return connection;
}

FdWatchMap* register_watch_functions_default(DBusConnection *conn)
{
    FdWatchMap *data = NULL;
    
    if (NULL == conn)
    {
        return NULL;
    }

    data = &_fd_poll;
    memset(data, 0, sizeof(FdWatchMap));
    
    if (!dbus_connection_set_watch_functions(conn, add_watch_default,
                 remove_watch_default, toggle_watch_default, data, NULL)) 
    {
        dbg(IOT_ERROR, "failed, out of memory");
        data = NULL;
    }
    return data;
}


/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
dbus_bool_t register_watch_functions(DBusConnection *conn, WatchFuncsCallback *w)
{
    FdWatchMap* watch_data;

    if ((NULL == conn) || (NULL == w))
    {
        dbg(IOT_ERROR, "connection is null");
        return FALSE;
    }

    if (!dbus_connection_set_watch_functions(conn, w->add_watch_handler,
                 w->remove_watch_handler, w->toggled_watch_hadler, w->data, w->free_func_handler)) 
    {
            dbg(IOT_ERROR, "failed, out of memory");
            return FALSE;
    }
    return TRUE;
}




/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
dbus_bool_t register_filter_functions(DBusConnection *conn, FilterFuncsCallback *f)
{
    if ((NULL == conn) || (NULL == f))
    {
        return FALSE;
    }

    if (!dbus_connection_add_filter(conn, f->message_handler, f->data, f->free_func_handler))
    {
        dbg(IOT_ERROR, "failed, out of memory");
        return FALSE;
    }

    return TRUE;
}

static void poll_handler(DBusConnection *conn, short events, DBusWatch *watch)
{
    unsigned int flags = 0;
    int process_count = 1;
    printf("%s - events = 0x%x\n", __func__, events);
    if (events & POLLIN) 
        flags |= DBUS_WATCH_READABLE;
    if (events & POLLOUT)
        flags |= DBUS_WATCH_WRITABLE;
    if (events & POLLHUP)
        flags |= DBUS_WATCH_HANGUP;
    if (events & POLLERR)
        flags |= DBUS_WATCH_ERROR;
    while (!dbus_watch_handle(watch, flags)) 
    {
        dbg(IOT_WARNING, "dbus_watch_handle needs more memory");
        sleep(1);
    }
   
    dbus_connection_ref(conn);
    while (dbus_connection_dispatch(conn) == DBUS_DISPATCH_DATA_REMAINS);
    dbus_connection_unref(conn);
}

void dbus_loop_start(DBusConnection *conn, void *data)
{
    int32_t i, nfds;
    FdWatchMap p, *in;
    if ((NULL == data) || (NULL == conn))
    {
        dbg(IOT_ERROR, "loop start fail, check incoming params");
        return ;
    }
    memset(&p, 0, sizeof(p));
    in = (FdWatchMap *)data;

    for (nfds = i = 0; i < in->max_fd; ++i) 
    {
        if (!dbus_watch_get_enabled(in->watches[i])) 
        {
            dbg(IOT_DEBUG, "watchs[%d]=%p not enabled,", i, in->watches[i]);
            continue;
        }
        p.pollfds[nfds].fd = in->pollfds[i].fd;
        p.pollfds[nfds].events = in->pollfds[i].events;
        p.pollfds[nfds].revents = 0;
        p.watches[nfds] = in->watches[i];
        ++nfds;
        p.max_fd = nfds;
    }

    for (i=0; i<nfds; i++)
    {
        dbg(IOT_DEBUG, "%d: fd = %d, events = 0x%x\n", i, p.pollfds[i].fd, p.pollfds[i].events);
    }

    //printf("loop started...\n");

    while (1)
    {
        if (poll(p.pollfds, p.max_fd, -1) <= 0) 
        {
            dbg(IOT_ERROR, "poll error\n");
            break;
        }
        for (i = 0; i < p.max_fd; i++) 
        {
            if (p.pollfds[i].revents) 
            {
                poll_handler(conn, p.pollfds[i].revents, p.watches[i]);
            }
        }
    }
}

void dbus_add_match(DBusConnection *connection, const char *rule)
{
    // rule = type='signal', interface='org.share.linux'
    dbus_bus_add_match(connection, rule, NULL);
}



