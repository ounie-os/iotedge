#include "modbus.h"
#include "modbus-tcp.h"
#include "dbus-base.h"
#include "logger.h"

int main(int argc, char **argv)
{
    modbus_t *ctx;
    int rc;
    uint16_t tab_reg[20] = {0};
    int regs_read_count = 0, seq = 0, i;

    if (argc == 1)
    {
        return 1;
    }

    ctx = modbus_new_tcp(argv[1], 502);
    if (NULL == ctx)
    {
        dbg(LOG_ERROR, "ctx is null");
        return 1;
    }
    
    rc = modbus_connect(ctx);
    if (-1 == rc)
    {
        dbg(LOG_ERROR, "modbus connect %s fail", argv[1]);
        modbus_close(ctx);
        modbus_free(ctx);
        return 1;
    }

    modbus_set_response_timeout(ctx, 3, 0);

    while (1)
    {
        //modbus_set_slave(ctx, 3);
        regs_read_count = modbus_read_registers(ctx, 0, 3, tab_reg);
        if (-1 == regs_read_count)
        {
            dbg(LOG_WARNING, "slave 1 read error");
        }
        else
        {
            dbg(LOG_DEBUG, "[%4d][1 read num = %d]", seq++, regs_read_count);
            for (i=0; i<regs_read_count; i++)
            {
                printf("<%#x> ", tab_reg[i]);
            }
            printf("\n");
        }
        sleep(1);
        
    }
    
}
