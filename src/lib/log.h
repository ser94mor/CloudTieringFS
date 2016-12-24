#ifndef LOG_H
#define LOG_H

/**
 * SYSLOG
 */

/* inclusion is needed for syslog's logging levels and functions' signatures */
#include <syslog.h>

/* logging levels */
#define syslog_ERROR    LOG_ERR
#define syslog_INFO     LOG_INFO
#define syslog_DEBUG    LOG_DEBUG

/* logging functions */
void syslog_open_log(const char *name);
#define syslog_log         syslog
#define syslog_close_log   closelog


/**
 * SIMPLE
 */

/* logging levels */
#define simple_ERROR    0
#define simple_INFO     3
#define simple_DEBUG    5

/* logging functions */
void simple_open_log(const char *name);
void simple_log(int level, const char *msg_fmt, ...);
void simple_close_log(void);

#endif /* LOG_H */
