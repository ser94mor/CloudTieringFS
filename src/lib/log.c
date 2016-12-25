/**
 * Copyright (C) 2016  Sergey Morozov <sergey94morozov@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "log.h"


/*
 * Functions and variables for 'syslog'.
 */

/**
 * @brief syslog_open_log Implementation of open_log function using syslog.
 * @param name Name prepended to every message (usually program name).
 */
void syslog_open_log(const char *name) {
        openlog(name, LOG_PID, LOG_DAEMON);
}


/*
 * Functions and variables for 'simple'.
 */
static pthread_mutex_t simple_mutex;
static unsigned long simple_seq_cnt = 0;
static FILE *simple_stream = NULL;
static const char *simple_process_name = "unknown";

/**
 * @brief simple_open_log Simple implementation of open_log function.
 * @param name Name prepended to every message (usually program name).
 */
void simple_open_log(const char *name) {
        /* initialize mutex */
        simple_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        /* construct log file name */
        const char *file_ext = ".log";
        char filename[strlen(name) + strlen(file_ext) + 1];
        sprintf(filename, "%s%s", name, file_ext);

        pthread_mutex_lock(&simple_mutex);
        /* initialize process name and open file stream */
	simple_process_name = (name == NULL) ? "unknown" : name;
        simple_stream = fopen(filename, "a");

        if (!simple_stream) {
                /* if unable to open log file, write to stdout */
                simple_stream = stdout;
                pthread_mutex_unlock(&simple_mutex);
                simple_log(simple_ERROR,
                             "unable to open file %s in append mode for "
                             "logging; log to stdout instead",
                             filename);
                return;
        }
        pthread_mutex_unlock(&simple_mutex);
}

/**
 * @brief simple_log Simple implementation of log function.
 * @param level Not used.
 * @param msg_fmt Not used.
 * @param ... Not used.
 */
void simple_log(int level, const char *msg_fmt, ...) {
        va_list args;
        va_start(args, msg_fmt);

        const char *prefix = (level == simple_ERROR) ?
                                 "ERROR" :
                                 (level == simple_INFO) ? "INFO" : "DEBUG";

        if (!simple_stream) {
                simple_open_log("cloudtiering_UNKNOWN");
        }

        pthread_mutex_lock(&simple_mutex);
        fprintf(simple_stream, "%lu %s [%s]: ", simple_seq_cnt++, prefix, simple_process_name);
        vfprintf(simple_stream, msg_fmt, args);
        fprintf(simple_stream, "\n");

        fflush(simple_stream);
        pthread_mutex_unlock(&simple_mutex);

        va_end(args);
}

/**
 * @brief simple_close_log Simple implementation of close_log function.
 */
void simple_close_log(void) {
        pthread_mutex_lock(&simple_mutex);

        if (fclose(simple_stream)) {
                /* just ignore this error; it is impossible to recover */
        }

        pthread_mutex_unlock(&simple_mutex);
}
