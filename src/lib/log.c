#define __USE_BSD    /* needed for vsyslog */
#define _BSD_SOURCE

#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <pthread.h>

#include "log.h"


static pthread_mutex_t _default_mutex;
static unsigned long _default_seq_cnt = 0;

/*
 * Functions and variables for 'syslog'.
 */

/**
 * @brief _syslog_open_log Implementation of open_log function using syslog.
 * @param name Name prepended to every message (usually program name).
 */
void _syslog_open_log(const char *name) {
        openlog(name, LOG_PID, LOG_DAEMON);
}

/**
 * @brief _syslog_log Implementation of log function using syslog.
 * @param level Logging level.
 * @param msg_fmt Message format.
 * @param ... Values to be substituted to the provided message format.
 */
void _syslog_log(int level, const char *msg_fmt, ...) {
        va_list args;
        va_start(args, msg_fmt);

        vsyslog(level, msg_fmt, args);

        va_end(args);
}

/**
 * @brief _syslog_close_log Implementation of close_log function using syslog.
 */
void _syslog_close_log(void) {
        closelog();
}


/*
 * Functions for 'default'.
 */

/**
 * @brief default_open_log Default implementation of open_log function. Does nothing.
 * @param name Not used.
 */
void _default_open_log(const char *name) {
        _default_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
}

/**
 * @brief default_log Default implementation of log function. Does nothing.
 * @param level Not used.
 * @param msg_fmt Not used.
 * @param ... Not used.
 */
void _default_log(int level, const char *msg_fmt, ...) {
        va_list args;
        va_start(args, msg_fmt);

        FILE *stream = (level == _default_ERROR) ? stderr : stdout;
        const char *prefix = (level == _default_ERROR) ?
                                 "ERROR" :
                                 (level == _default_INFO) ? "INFO" : "DEBUG";


        pthread_mutex_lock(&_default_mutex);
        fprintf(stream, "%lu %s: ", _default_seq_cnt++, prefix);
        vfprintf(stream, msg_fmt, args);
        fprintf(stream, "\n");

        fflush(stream);
        pthread_mutex_unlock(&_default_mutex);

        va_end(args);
}

/**
 * @brief default_close_log Default implementation of close_log function. Does nothing.
 */
void _default_close_log(void) {
        /* no action */
}
