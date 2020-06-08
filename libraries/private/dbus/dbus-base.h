#ifndef __DBUS_BASE_H_
#define __DBUS_BASE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>
#include <sys/poll.h>


#define MAX_WATCHES 8

/** data struct defined */
typedef struct
{
    DBusAddWatchFunction add_watch_handler;
    DBusRemoveWatchFunction remove_watch_handler;
    DBusWatchToggledFunction toggled_watch_hadler;
    DBusFreeFunction free_func_handler;
    void *data;
}WatchFuncsCallback;

typedef struct
{
    DBusHandleMessageFunction message_handler;
    DBusFreeFunction free_func_handler;
    void *data;
}FilterFuncsCallback;

typedef struct
{
    struct pollfd pollfds[MAX_WATCHES];
    dbus_unichar_t max_fd;
    DBusWatch *watches[MAX_WATCHES];
}FdWatchMap;

typedef struct
{
    const char *bus_path;
    const char *obj_path;
    const char *interface_path;
    union {
        const char *method_name;
        const char *signal_name;
    }act;
}IpcPath;

#if 0
typedef struct
{
    int interval;
    DBusTimeoutHandler handler;
    void *data;
    DBusFreeFunction free_data_function;
}TimeOutItem;
#endif /* 0 */

typedef struct
{
    DBusAddTimeoutFunction add_timeout_handle;
    DBusRemoveTimeoutFunction remove_timeout_handle;
    DBusTimeoutToggledFunction toggle_timeout_handle;
    DBusFreeFunction free_func_handler;
    void *data;
}TimeOutCallback;

/** api declare */
DBusConnection *connect_dbus(const char *bus_name, int32_t *rc);
dbus_bool_t register_watch_functions(DBusConnection *conn, WatchFuncsCallback *w);
FdWatchMap* register_watch_functions_default(DBusConnection *conn);
dbus_bool_t register_filter_functions(DBusConnection *conn, FilterFuncsCallback *f);
void dbus_loop_start(DBusConnection *conn, void *data);

dbus_bool_t add_watch_default(DBusWatch *watch, void *data);
void remove_watch_default(DBusWatch *watch, void *data);
void toggle_watch_default(DBusWatch *watch, void *data);

dbus_bool_t send_method_call_with_reply(DBusConnection *connection, IpcPath *path_set, const char *content, char *reply_content);
dbus_bool_t send_method_call_without_reply(DBusConnection *connection, IpcPath *path_set, int dbus_type, const void *content);
dbus_bool_t send_signal(DBusConnection *connection, IpcPath *path_set, int dbus_type, const void *sigval);
void get_single_arg_from_message(DBusMessage* m, void *param);
dbus_bool_t send_method_return(DBusConnection *connection, DBusMessage *m, int dbus_type, const char *str);
dbus_bool_t register_timeout_functions(DBusConnection *connection, void *data);
void dbus_add_match(DBusConnection *connection, const char *rule);




#endif
