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

#ifndef CLOUDTIERING_LOG_INTERNAL_H
#define CLOUDTIERING_LOG_INTERNAL_H

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

#endif /* CLOUDTIERING_LOG_INTERNAL_H */
