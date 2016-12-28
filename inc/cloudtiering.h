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

#ifndef CLOUDTIERING_H
#define CLOUDTIERING_H


/* general macroses useful for definitions and declarations */
#define COMMA                ,
#define SEMICOLON            ;
#define PLUS                 +
#define ENUMERIZE(elem)      e_##elem
#define STRINGIFY(elem)      #elem
#define MAP_TO_ONE(elem)     1


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

/* suffix for variable name of specific logging framework */
#define LOG_VAR_SUFFIX    _logger

/* macros that is useful for initialization of specific logging frameworks */
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

/* macro to get an address of a variable name of specific logging framework */
#define LOGGER_ADDR(elem)    &elem##LOG_VAR_SUFFIX

/* macros which defines a list of supported logging frameworks
   and allows to declare enums, arrays and perform variables' declarations */
#define LOGGERS(action, sep)   \
            action(syslog) sep \
            action(simple)

/* enum of supported logging frameworks */
enum log_e {
        LOGGERS(ENUMERIZE, COMMA)
};

/* definition of framework-agnostic logger */
typedef struct {
        /* logging framework type */
        enum log_e type;

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


/*******************************************************************************
* QUEUE                                                                        *
* -----                                                                        *
*******************************************************************************/

/* inclusions of third-parties */
#include <pthread.h>      /* included for a pthread_mutex_t type definition */
#include <sys/types.h>    /* included for a size_t type definition */

/* a definition of a queue data-structure */
typedef struct {
        /* a pointer to the front element of a queue */
        char  *head;

        /* a pointer to a back element of a queue */
        char  *tail;

        /* a current queue's size */
        size_t cur_size;

        /* a maximum queue's size */
        size_t max_size;

        /* a data's maximum size */
        size_t data_max_size;

        /* a pointer to a buffer where elements are stored */
        const char  *buf;

        /* a size in byte of a buffer where elements are stored */
        size_t buf_size;

        /* a mutex to ensure a thread-safety property */
        pthread_mutex_t mutex;
} queue_t;

int queue_empty(queue_t *queue);
int queue_full(queue_t *queue);

int queue_push(queue_t *queue, const char *data, size_t data_size);
int queue_pop(queue_t *queue);

int queue_front(queue_t *queue, char *data, size_t *data_size);

queue_t *queue_alloc(size_t queue_max_size, size_t data_max_size);
void queue_free(queue_t *queue);


/*
 * OPS
 */

typedef struct {
        int  (*init_remote_store_access)(void);
        int  (*move_file_in)(const char *path);
        int  (*move_file_out)(const char *path);
        void (*term_remote_store_access)(void);
} ops_t;

int move_file(queue_t *queue);


/*
 * CONF
 */

#include <stddef.h>
#include <time.h>

typedef struct {
        /* 4096 is minimum acceptable value (including null) according to POSIX (equals PATH_MAX from linux/limits.h) */
        char   fs_mount_point[4096];          /* filesystem's root directory */

        time_t scanfs_iter_tm_sec;            /* the lowest time interval between file system scan iterations */
        int    scanfs_max_fails;              /* the maximum number of allowed failures of scanfs execution */
        int    move_file_max_fails;           /* the maximum number of allowed failures of file_move execution */
        double move_out_start_rate;           /* start evicting files when storage is (move_out_start_rate * 100)% full */
        double move_out_stop_rate;            /* stop evicting files when storage is (move_out_stop_rate * 100)% full */
        size_t out_q_max_size;                /* maximum size of out queue */
        size_t in_q_max_size;                 /* maximum size of in queue */
        size_t path_max;                      /* maximum path length in fs_mount_point directory can not be lower than this value */
        ops_t  ops;                           /* operation (depends on remote store protocol) */

        /* 63 it is unlikely that protocol identifies require more symbols */
        char   remote_store_protocol[64];

        /* 15 enough to store string id of transfer protocol */
        char transfer_protocol[16];

        /* 255 equals to S3_MAX_HOSTNAME_SIZE from libs3 */
        char   s3_default_hostname[256];      /* s3 default hostname (libs3) */

        /* 63 is maximum allowed bucket name according to http://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html */
        char   s3_bucket[64];                 /* name of s3 bucket which will be used as a remote tier */

        /* 32 is maximum access key id length according to http://docs.aws.amazon.com/IAM/latest/APIReference/API_AccessKey.html */
        char   s3_access_key_id[33];          /* s3 access key id */

        /* 127 is an assumption that in practice longer keys do not exist */
        char   s3_secret_access_key[128];     /* s3 secret access key */
} conf_t;

int read_conf(const char *conf_path);
conf_t *get_conf();
log_t  *get_log();
ops_t  *get_ops();


/*
 * SCANFS
 */

int scanfs(queue_t *in_q, queue_t *out_q);

#endif /* CLOUDTIERING_H */
