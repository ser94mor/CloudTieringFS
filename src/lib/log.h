#ifndef LOG_H
#define LOG_H

/**
 * SYSLOG
 */
#include <syslog.h>

#define SYSLOG_ERROR    LOG_ERR
#define SYSLOG_INFO     LOG_INFO
#define SYSLOG_DEBUG    LOG_DEBUG

void syslog_open_log(const char *name);
void syslog_log(int level, const char *msg_fmt, ...);
void syslog_close_log(void);


/**
 * DEFAULT
 */
#define     DEFAULT_ERROR    0
#define     DEFAULT_INFO     3
#define     DEFAULT_DEBUG    5

void default_open_log(const char *name);
void default_log(int level, const char *msg_fmt, ...);
void default_close_log(void);

#endif /* LOG_H */
