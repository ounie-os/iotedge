#include "modbus.h"
#include "modbus-rtu.h"
#include "dbus-base.h"
#include "logger.h"

int main(int argc, char **argv)
{
    const char *port = argv[1];
    int baud = atoi(argv[2]);
    modbus_t *ctx;
    int rc;
    uint16_t tab_reg[10] = {0};
    int regs_read_count = 0, seq = 0, i;

    if (argc == 1)
    {
        dbg(IOT_WARNING, "need more params");
        return 1;
    }
    
    ctx = modbus_new_rtu(port, baud, 'N', 8, 1);
    if (NULL == ctx)
    {
        dbg(IOT_ERROR, "modbus_new_rtu fail");
        return 1;
    }

    //modbus_set_slave(ctx, int slave)
    rc = modbus_connect(ctx);
    if (-1 == rc)
    {
        dbg(IOT_ERROR, "modbus connect %s fail", argv[1]);
        modbus_close(ctx);
        modbus_free(ctx);
        return 1;
    }

    modbus_set_response_timeout(ctx, 2, 0);

    while (1)
    {
        modbus_set_slave(ctx, 3);
        regs_read_count = modbus_read_registers(ctx, 0, 3, tab_reg);
        if (-1 == regs_read_count)
        {
            dbg(IOT_WARNING, "slave 1 read error");
        }
        else
        {
            dbg(IOT_DEBUG, "[%4d][1 read num = %d]", seq, regs_read_count);
            for (i=0; i<regs_read_count; i++)
            {
                printf("<%#x> ", tab_reg[i]);
            }
            printf("\n");
        }

        modbus_set_slave(ctx, 4);
        regs_read_count = modbus_read_registers(ctx, 0, 3, tab_reg);
        if (-1 == regs_read_count)
        {
            dbg(IOT_WARNING, "slave 2 read error");
        }
        else
        {
            dbg(IOT_DEBUG, "[%4d][2 read num = %d]", seq++, regs_read_count);
            for (i=0; i<regs_read_count; i++)
            {
                printf("<%#x> ", tab_reg[i]);
            }
            printf("\n");
        }
        sleep(1);

    }
    
}

