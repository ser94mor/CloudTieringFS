/**
 * Copyright (C) 2016, 2017  Sergey Morozov <ser94mor@gmail.com>
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

#include "log_internal.h"


/*
 * Functions and variables for 'syslog' logging framework.
 */

/**
 * @brief syslog_open_log Implementation of open_log() function using syslog.
 *
 * @param[in] name A string prepended to every message (usually a program name).
 */
void syslog_open_log(const char *name) {
        openlog(name, LOG_PID, LOG_DAEMON);
}


/*
 * Functions and variables for 'simple' logging framework (log to file).
 */

/* global mutex to ensure log records consistency */
static pthread_mutex_t simple_mutex;

/* number of currently recorded messages */
static unsigned long simple_seq_cnt = 0;

/* FILE stream where to write log messages */
static FILE *simple_stream = NULL;

/* process name; actual name will be assigned later;
   default value is provided for a fallback case */
static const char *simple_process_name = "unknown";

/**
 * @brief simple_open_log Simple implementation of open_log() function.
 *
 * @param[in] name A string prepended to every message (usually a program name).
 */
void simple_open_log(const char *name) {
        /* initialize mutex */
        simple_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        pthread_mutex_lock(&simple_mutex);

        /* initialize process name and open file stream */
        simple_process_name = (name == NULL) ? "unknown" : name;

        /* construct log file name */
        const char *file_ext = ".log";
        char filename[strlen(simple_process_name) + strlen(file_ext) + 1];
        sprintf(filename, "%s%s", simple_process_name, file_ext);

        /* open file where log messages should be recorded */
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
 * @brief simple_log Simple implementation of log() function.
 *
 * @param[in] level   A logging level.
 * @param[in] msg_fmt A message string, possibly with specifiers.
 * @param[in] ...     Values to be substitures to msg_fmt string.
 */
void simple_log(int level, const char *msg_fmt, ...) {
        va_list args;
        va_start(args, msg_fmt);

        /* construct message prefix based on logging level value */
        const char *prefix = (level == simple_ERROR) ?
                                 "ERROR" :
                                 (level == simple_INFO) ? "INFO" : "DEBUG";

        /* open a file stream if not opened already */
        if (!simple_stream) {
                simple_open_log(NULL);
        }

        pthread_mutex_lock(&simple_mutex);

        /* write message prefix, message and new line character at the end */
        fprintf(simple_stream, "%lu %s [%s]: ", simple_seq_cnt++, prefix, simple_process_name);
        vfprintf(simple_stream, msg_fmt, args);
        fprintf(simple_stream, "\n");

        /* flush stream's buffer immediately */
        fflush(simple_stream);

        pthread_mutex_unlock(&simple_mutex);

        va_end(args);
}

/**
 * @brief simple_close_log Simple implementation of close_log() function.
 */
void simple_close_log(void) {
        pthread_mutex_lock(&simple_mutex);

        if (fclose(simple_stream)) {
                /* just ignore this error; it is impossible to recover */
        }

        pthread_mutex_unlock(&simple_mutex);
}
