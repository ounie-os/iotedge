#ifndef __LOGGER_H_
#define __LOGGER_H_
#include <stdlib.h>

typedef enum
{
    IOT_ERROR = 0,
    IOT_WARNING = 1,
    IOT_DEBUG = 2
}iot_log_t;

//#define _dbgdump printf
#define _dbgdump(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)


#define dbg(level, fmt, ...) \
do { \
    switch (level) { \
        case 0: \
            _dbgdump("<ERROR>-[%s][%s][%d]-" fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__);  break; \
        case 1: \
            _dbgdump("<WARNING>-[%s][%s][%d]-" fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__);  break; \
        case 2: \
            _dbgdump("<DEBUG>-[%s][%s][%d]-" fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__);  break; \
        default: break; \
        } \
} while (0)

#endif
