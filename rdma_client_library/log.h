#ifndef _LOG_H_
#define _LOG_H_

#define LOG_INFO 5
#define DEBUG

#ifndef LOG_LEVEL
#ifdef DEBUG
#define LOG_LEVEL 10
#else // !DEBUG
#define LOG_LEVEL 1
#endif // DEBUG
#endif // LOG_LEVEL

#define LOG_KERN(LEVEL, STR) do {\
        if ((LEVEL) < LOG_LEVEL) {\
            printk(KERN_WARNING "%-30s | %3d |  ",__FUNCTION__,__LINE__); \
            printk STR; \
        } \
} while(0);

#endif // _LOG_H_

