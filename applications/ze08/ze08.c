#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pthread.h>
#include "serial.h"
#include "stdint.h"
#include "dbus-base.h"
#include "logger.h"
#include "dbus_ipc_name.h"
#include "parse-config.h"


#define LCD1602_DEVNAME "/dev/lcd1602"
#define ZE08_CONFIG_FILE "/mnt/ze08.json"


#define LCD_CLR _IO('L',0)
#define LCD_ONE _IO('L',1)
#define LCD_TWO _IO('L',2)
#define LCD_SET _IO('L',3)
#define LCD_COM _IO('L',4)
#define LCD_AC  _IO('L',5)

struct serial_params
{
    char *device;
    int baudrate;
    DBusConnection *conn;
};

static DBusHandlerResult zeo8_filter_func(DBusConnection *c, DBusMessage *m, void *data);

static FilterFuncsCallback ze08_msg_handler = {
            .message_handler = zeo8_filter_func,
            .free_func_handler = NULL,
            .data = NULL};


static int loop = 1;
static float g_ppm = 0;
static int ze08_process_flag = 1;
static cJSON* json = NULL;

static DBusHandlerResult zeo8_filter_func(DBusConnection *c, DBusMessage *m, void *data)
{
    const char *iface_path, *member_name;
    char ppm_str[16] = {0};
    char *p_ppm = ppm_str;
    iface_path = dbus_message_get_interface(m);
    member_name = dbus_message_get_member(m);
    
    dbg(IOT_DEBUG, "if:%s, method:%s, message type = %d", iface_path, member_name, dbus_message_get_type(m));
    if ((NULL == iface_path) || (NULL == member_name))
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (0 == strcmp(iface_path, ZE08_IFACE_PATH))
    {
        snprintf(ppm_str, 16, "%.6f", g_ppm);
        if (FALSE == send_method_return(c, m, DBUS_TYPE_STRING, &p_ppm))
        {
            dbg(IOT_WARNING, "send method fail: [%s] to be sent", ppm_str);
        }
        dbg(IOT_DEBUG, "send method ok: [%s] sent", ppm_str);
    }
    return DBUS_HANDLER_RESULT_HANDLED;

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


#if 0
static int ze08_config(cJSON *j, const char *string_name)
{
    cJSON* c = cJSON_GetObjectItem(j, string_name);
    int result = (int)(c->valuedouble);
    return result;
}
#endif /* 0 */

static int ze08_config(void *data, int argc, char **argv, char **azColName)
{
    int i = 0;
    for (i = 0; i < argc; i++) 
    {
        if (0 == strcmp(azColName[i], "warning_flag"))
        {
            strcpy(data, argv[i]);
            break;
        }
    }
    return 0;
}

static unsigned char FuncCheckSum(unsigned char *buf, unsigned char len)
{
    unsigned char j, checksum=0;
    buf += 1;

    for (j=0; j < (len-2); j++)
    {
        checksum += *buf;
        buf++;
    }
    checksum = (~checksum) + 1;
    return checksum;
}

void *ze08_process(void *arg)
{
    int fd, lcd_fd, i, rd, count = 0;
    char c;
    char buf[16] = {0};
    char ppm_str[16] = {0};
    char *p_ppm = ppm_str;
    unsigned short concentration;
    unsigned short concentration_low, concentration_high;
    float ppm;
    const char *gas_name = "0.08mg/m3";
    const char *unit = " mg/m3";
    char tmp[2] = {0xdf, 'C'};
    char space = ' ';
    char percent = '%';
    float temp1 = 0, humidity1 = 0;
    char temp1_f[8] = {0};
    char humidity1_f[8] = {0};
    int alarm_count = 0, alarm_threshold = 0;
    int ch2o_over_new = 0, ch2o_over_old = 0;
    int warning_flag = 0;
    unsigned char checksum = 0;
    
    struct serial_params *serial = (struct serial_params *)arg;

#if 0
    json = get_file_to_json(ZE08_CONFIG_FILE);
#endif /* 0 */

    fd = serial_open_file(serial->device, serial->baudrate);
    lcd_fd = open(LCD1602_DEVNAME, O_RDWR);
    if (fd > 0) 
    {		
        if (lcd_fd > 0)
        {
            serial_set_attr(fd, 8, PARITY_NONE, 1, FLOW_CONTROL_NONE);
            ioctl(lcd_fd, LCD_CLR);
            ioctl(lcd_fd, LCD_COM, 0x0c);
            //ioctl(lcd_fd, LCD_SET, 0);
            //write(lcd_fd, gas_name, strlen(gas_name));
            //write(lcd_fd, &space, 1);
            ioctl(lcd_fd, LCD_SET, 0x40+8);
            write(lcd_fd, unit, strlen(unit));
            dbg(IOT_DEBUG, "ze08 thread start...");
            while (loop) 
            {			
                rd = serial_receive(fd, &c, 1);
                //dbg(IOT_DEBUG, "count = %d, read = %d", count, c);
                if (rd == 1)
                {
                    // ÆðÊ¼Î»
                    if (c == 0xff)
                    {
                        count = 0;
                        buf[count] = c;
                        count++;
                    }
                    while (count)
                    {
                        rd = serial_receive(fd, &c, 1);
                        if (rd == 1)
                        {
                    buf[count] = c;
                    count++;
                    if (count % 9 == 0) 
                    {
                                count = 0;
                        checksum = FuncCheckSum(buf, 9);
                        if (checksum == buf[8])
                        {
#if 0
                            for (i=0; i<9; i++)
                            {
                                printf("0x%02x ", buf[i]);
                            }
                            printf("\n");
#endif /* 0 */

                            concentration_high = buf[4];
                            concentration_low = buf[5];
                            concentration = (concentration_high << 8) + concentration_low; // ppb
                                    dbg(IOT_WARNING, "concentration_high=%u(%x),concentration_low=%u(%x), concentration=%u(%x)", 
                                    concentration_high, concentration_high, concentration_low, concentration_low, concentration, concentration);
                                    if (concentration > 5000) {
                                        concentration = 5000;
                                    }
                            ppm = (float)concentration / 1000 * 1.25;
                                    dbg(IOT_WARNING, "ppm = %f", ppm);
                            snprintf(ppm_str, 16, "%.6f", ppm);
                            ioctl(lcd_fd, LCD_SET, 0x40);
                            write(lcd_fd, ppm_str, strlen(ppm_str));
                            ioctl(lcd_fd, LCD_SET, 0);
                            temp1 = (float)get_result_from_shtc1("/sys/class/hwmon/hwmon0/temp1_input") / 1000;
                            humidity1 = (float)get_result_from_shtc1("/sys/class/hwmon/hwmon0/humidity1_input") / 1000;
                            snprintf(humidity1_f, 8, "%.1f", humidity1);
                            write(lcd_fd, humidity1_f, strlen(humidity1_f));
                            write(lcd_fd, &percent, 1);
                            write(lcd_fd, &space, 1);
                            snprintf(temp1_f, 8, "%.1f", temp1);
                            write(lcd_fd, temp1_f, strlen(temp1_f));
                            for (i=0; i<2; i++)
                            {
                                write(lcd_fd, &tmp[i], 1);
                            }
                            g_ppm = ppm;
                            const char *sql = "SELECT warning_flag from ze08 where id = 1;";
                            op_sqlite_db(DB_PATH, sql, ze08_config, (void *)&warning_flag);
                            if ((ppm > 0.08) && (1 == warning_flag))
                            {   
                                IpcPath emit_path_set = {
                                        .bus_path = ZE08_BUS_NAME,
                                        .obj_path = ZE08_OBJ_PATH,
                                        .interface_path = ZE08_IFACE_PATH,
                                        .act.signal_name = "ch2o_warning"
                                    };

                                 IpcPath emit_path_set1 = {
                                        .bus_path = ZE08_BUS_NAME,
                                        .obj_path = ZE08_OBJ_PATH,
                                        .interface_path = ZE08_IFACE_PATH,
                                        .act.signal_name = "ch2o_warning_float"
                                    };
                                //dbg(IOT_DEBUG, "ppm = (0x%x)(%f)", g_ppm, g_ppm);
#if 0
                                alarm_count++;
                                alarm_threshold = ze08_config(json, "threshold_count");
                                if (alarm_count >= alarm_threshold)
                                {
                                    send_signal(serial->conn, &emit_path_set, DBUS_TYPE_STRING, &p_ppm);
                                    send_signal(serial->conn, &emit_path_set1, DBUS_TYPE_UINT32, &g_ppm);
                                    alarm_count = 0;
                                }
#endif /* 0 */
                                ch2o_over_new = 1;
                                if (ch2o_over_old ^ ch2o_over_new)
                                {
                                    if (ch2o_over_new)
                                    {
                                        send_signal(serial->conn, &emit_path_set, DBUS_TYPE_STRING, &p_ppm);
                                        send_signal(serial->conn, &emit_path_set1, DBUS_TYPE_UINT32, &g_ppm);
                                    }
                                    ch2o_over_old = ch2o_over_new;
                                }
                            }
                            else
                            {
                                //alarm_count = 0;
                                ch2o_over_new = 0;
                            }
                        }
                        else
                        {
                            fprintf(stdout, "cal checksum=%d, ori checksum=%d", checksum, buf[8]);
                                }
                            }
                        }
                    }
                }
                else
                {
                    dbg(IOT_WARNING, "serial read timeout");
                }	
            }
            close(lcd_fd);
        }
        else
        {
            dbg(IOT_ERROR, "lcd 1602 not ready");
        }
        serial_close(fd);	
    }
    else
    {
        dbg(IOT_ERROR, "serial not ready");
    }
    ze08_process_flag = 0;

}

static void cfinish(int sig)
{	
    signal(SIGINT, NULL);
    printf("finished\n");
    loop = 0;
}

int main(int argc, char** argv) 
{	
    int fd, lcd_fd, i, rd, count = 0;	
    char c;
    char buf[16] = {0};
    char ppm_str[16] = {0};
    unsigned int concentration, range;
    unsigned int concentration_low, concentration_high, range_low, range_high;
    float ppm;
    const char *gas_name = "CH2O:0.08mg/m3";
    const char *unit = " mg/m3";

    DBusConnection *conn;
    int32_t rc = 0;
    FdWatchMap *poll_data;
    dbus_bool_t ret = FALSE;
    pthread_t ntid;

    struct serial_params serial;
    
    
    if (argc < 3)	
    {		
        printf("you should do like this: \n");		
        printf("./test /dev/ttyO2 115200\n");		
        exit(1);	
    }

    conn = connect_dbus(ZE08_BUS_NAME, &rc);
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
    ret = register_filter_functions(conn, &ze08_msg_handler);
    if (ret == FALSE)
    {
        printf("Failed to register msg handler callback\n");
        exit(1);
    }

    signal(SIGINT, cfinish);

    serial.device = argv[1];
    serial.baudrate = atoi(argv[2]);
    serial.conn = conn;

    rc = pthread_create(&ntid, NULL, ze08_process, (void *)&serial);
    if (rc != 0)
    {
        dbg(IOT_ERROR, "pthread create fail");
        exit(1);
    }
    
    dbus_loop_start(conn, (void *)poll_data);

    while (ze08_process_flag)
    {
        sleep(1);
    }

    return 0;
    
}


