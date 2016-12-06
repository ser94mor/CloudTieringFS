#ifndef LOG_H
#define LOG_H

/**
 * SYSLOG
 */
#include <syslog.h>

#define _syslog_ERROR    LOG_ERR
#define _syslog_INFO     LOG_INFO
#define _syslog_DEBUG    LOG_DEBUG

void _syslog_open_log(const char *name);
void _syslog_log(int level, const char *msg_fmt, ...);
void _syslog_close_log(void);


/**
 * DEFAULT
 */
#define     _default_ERROR    0
#define     _default_INFO     3
#define     _default_DEBUG    5

void _default_open_log(const char *name);
void _default_log(int level, const char *msg_fmt, ...);
void _default_close_log(void);

#endif /* LOG_H */
