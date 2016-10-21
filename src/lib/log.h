#ifndef LOG_H
#define LOG_H

#include <syslog.h>

#define EMERG      LOG_EMERG
#define ALERT      LOG_ALERT
#define CRIT       LOG_CRIT
#define ERR        LOG_ERR
#define WARNING    LOG_WARNING
#define NOTICE     LOG_NOTICE
#define INFO       LOG_INFO
#define DEBUG      LOG_DEBUG

/* logger operations are wrapped by macroses to be able painlessly change logger type in the future */

#define OPEN_LOG() { \
            openlog("cloudtiering", LOG_PID, LOG_DAEMON); \
        }

#define LOG(level,format,args...) { \
            syslog(level, format, ## args); \
        }
        
#define CLOSE_LOG() { \
            closelog(); \
        }

#endif // LOG_H
