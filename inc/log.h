/**
 * Copyright (C) 2017  Sergey Morozov <sergey@morozov.ch>
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


#ifndef CLOUDTIERING_LOG_H
#define CLOUDTIERING_LOG_H

/*******************************************************************************
* LOGGING                                                                      *
* -------                                                                      *
*                                                                              *
* Framework-agnostic logging facility. Type definitions, function signatures'  *
* declarations and macroses that provide a user with short, concise and        *
* convenient logging tackle.                                                   *
*                                                                              *
* Usage:                                                                       *
*     OPEN_LOG("program_name");                                                *
*     ...                                                                      *
*     LOG(ERROR, "failure: %s", err_msg);                                      *
*     LOG(INFO,  "connection established successfully");                       *
*     LOG(ERROR, "counter value: %d", cnt_val);                                *
*     ...                                                                      *
*     CLOSE_LOG();                                                             *
*                                                                              *
* Notes:                                                                       *
*     - LOG() expands to function which is uaranteed to be thread-safe.        *
*     - Thread-safeness of OPEN_LOG() and CLOSE_LOG() expansions is not        *
*       guaranteed and depends on an actual logging framework implementation.  *
*******************************************************************************/

#include "defs.h"

/* suffix for variable name of specific logging framework */
#define LOG_VAR_SUFFIX    _logger

/* a macro-function to declare log_t data-structure for a given logger */
#define DECLARE_LOGGER(elem)                           \
                static log_t elem##LOG_VAR_SUFFIX = {  \
                        .type      = e_##elem,         \
                        .open_log  = elem##_open_log,  \
                        .log       = elem##_log,       \
                        .close_log = elem##_close_log, \
                        .error     = elem##_ERROR,     \
                        .info      = elem##_INFO,      \
                        .debug     = elem##_DEBUG      \
                }

/* a macro-function to get an address of a variable name of a given logger */
#define LOGGER_ADDR(elem)    &elem##LOG_VAR_SUFFIX

/* a list of supported loggers */
#define LOGGERS(action, sep) \
        action(syslog) sep   \
        action(simple)

/* enum of supported logging frameworks */
enum log_enum {
        LOGGERS(ENUMERIZE, COMMA),
};

/* definition of framework-agnostic logger */
typedef struct {
        /* logging framework type */
        enum log_enum type;

        /* framework-agnostic logging initializer */
        void (*open_log)(const char *);

        /* framework-agnostic logging function */
        void (*log)(int, const char *, ...);

        /* framework-agnostic logging destructor */
        void (*close_log)(void);

        /* number representing error message logging level */
        int  error;

        /* number representing info message logging level */
        int  info;

        /* number representing debug message logging level */
        int  debug;
} log_t;

/* concise macroses representing different logging levels */
#define ERROR      (get_log()->error)
#define INFO       (get_log()->info)
#define DEBUG      (get_log()->debug)

/* concise macroses representing logging functions' calls */
#define OPEN_LOG(name)            (get_log()->open_log(name))
#define LOG(level,format,args...) (get_log()->log(level, format, ## args))
#define CLOSE_LOG()               (get_log()->close_log())

log_t *get_log();

#endif    /* CLOUDTIERING_LOG_H */
