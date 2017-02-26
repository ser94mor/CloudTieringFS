/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey94morozov@gmail.com>
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


/*******************************************************************************
* COMMON CONSTANTS                                                             *
* ----------------                                                             *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

/* constant representing string with program name */
#define PROGRAM_NAME        "cloudtiering"

/* buffer length for error messages; mostly for errno descriptions */
#define ERR_MSG_BUF_LEN     1024


/*******************************************************************************
* MACROSES                                                                     *
* --------                                                                     *
*                                                                              *
* A creation of some commoly used definitions is simplified via macroses.      *
* A basic idea is to define a list of names of a similar purpose               *
* (i.e. loggers, protocols, configuration sections etc.) and automate creation *
* of corresponding variables, enums, arrays or anything with simple short      *
* macroses.                                                                    *
*                                                                              *
* A list of entities of some nature is usually defined in a folling way:       *
*         #define ENTITIES(action, sep) \                                      *
*                 action(foo) sep       \                                      *
*                 action(bar) sep       \                                      *
*                 action(goo)                                                  *
* Here 'action' is a macro-function which accepts entity name as an argument   *
* and replaces it with some other thing. 'sep' is a separator which should     *
* separate definitions produced by 'action' macro-function. Set of actions     *
* and separators will be defined in this file.                                 *
*                                                                              *
* Examples:                                                                    *
* (1) Enumerations, arrays:                                                    *
*         #define ENUMERIZE(elem)      e_##elem                                *
*         #define COMMA                ,                                       *
*                                                                              *
*         enum entity_e {                                                      *
*                 ENTITIES(ENUMERIZE, COMMA),                                  *
*         };                                                                   *
*     Preprocessor will transform it to:                                       *
*         enum entity_e {                                                      *
*                 e_foo,                                                       *
*                 e_bar,                                                       *
*                 e_goo,                                                       *
*         };                                                                   *
*                                                                              *
* (2) Number of elements in array, number of defined entities:                 *
*         #define MAP_TO_ONE(elem)     1                                       *
*         #define PLUS                 +                                       *
*                                                                              *
*         size_t entity_count = ENTITIES(MAP_TO_ONE, PLUS);                    *
*     Preprocessor will transform it to:                                       *
*         size_t entity_count = 1 + 1 + 1; // i.e. entity_count = 3            *
*                                                                              *
* (3) Variables' definitions:                                                  *
*         #define DECLARE_ENTITY(elem) const char *str_##elem = #elem          *
*         #define SEMICOLON            ;                                       *
*                                                                              *
*         ENTITIES(DECLARE_ENTITY, SEMICOLON);                                 *
*     Preprocessor will transform it to:                                       *
*         const char *str_foo = "foo";                                         *
*         const char *str_bar = "bar";                                         *
*         const char *str_goo = "goo";                                         *
*******************************************************************************/

/* common macroses representing separators */
#define COMMA                ,
#define SEMICOLON            ;
#define PLUS                 +
#define EMPTY

/* common macroses representing macro-functions */
#define ENUMERIZE(elem, ...)         e_##elem
#define STRINGIFY(elem, ...)         #elem
#define MAP_TO_ONE(elem, ...)        1


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


/*******************************************************************************
* QUEUE                                                                        *
* -----                                                                        *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

/* inclusions of third-parties */
#include <pthread.h>      /* included for a pthread_mutex_t type definition */
#include <linux/limits.h>
#include <sys/types.h>    /* included for a size_t type definition */

#define QUEUE_SHM_OBJ    "/cloudtiering"

/* a definition of a queue data structure */
typedef struct {
        size_t  head_offset;

        size_t  tail_offset;

        /* a current queue's size */
        size_t cur_size;

        /* a maximum queue's size */
        size_t max_size;

        /* a data's maximum size */
        size_t data_max_size;

        size_t buf_offset;

        /* a size in byte of a buffer where elements are stored */
        size_t buf_size;

        char shm_obj[NAME_MAX + 1];

        size_t total_size;

        /* a mutex to ensure a thread-safety property */
        pthread_mutex_t head_mutex;

        pthread_mutex_t tail_mutex;

        pthread_mutex_t size_mutex;

        pthread_cond_t fullness_cond;

        pthread_cond_t emptiness_cond;
} queue_t;

/* functions to work with queue_t data structure */
int  queue_init(queue_t **queue_p,
                size_t queue_max_size,
                size_t data_max_size,
                const char *shm_obj);

void queue_destroy(queue_t *queue);

int  queue_push(queue_t *queue,
                const char *data,
                size_t data_size);

int  queue_try_push(queue_t *queue,
                    const char *data,
                    size_t data_size);

int  queue_pop(queue_t *queue,
               char *data,
               size_t *data_size);

int  queue_try_pop(queue_t *queue,
                   char *data,
                   size_t *data_size);


/*******************************************************************************
* CONFIGURATION                                                                *
* -------------                                                                *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

/* a list of sections' names in configuration file */
#define SECTIONS(action, sep)     \
        action(General)      sep  \
        action(Internal)     sep  \
        action(S3RemoteStore)

/* a macro-function to declare begining and end tokens for a given section */
#define DECLARE_SECTION_STR(elem)                                      \
        static const char beg_##elem##_section_str[] = "<"  #elem ">", \
                          end_##elem##_section_str[] = "</" #elem ">"

/* a macro-function that produces dotconf's context value
   (it is greater than 0 because dotconf's CTX_ALL section macros is 0) */
#define SECTION_CTX(elem)     ENUMERIZE(elem) + 1

/* enum of supported sections in configuration */
enum section_enum {
        SECTIONS(ENUMERIZE, COMMA),
};


/*******************************************************************************
* OPERATIONS                                                                   *
* ----------                                                                   *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

/* a list of supported protocols */
#define PROTOCOLS(action, sep) \
        action(s3)

/* enum of supported protocols */
enum protocol_enum {
        PROTOCOLS(ENUMERIZE, COMMA),
};

/* a macro-function to declare ops_t data-structure for a given protocol */
#define DECLARE_OPS(elem)                                       \
        static ops_t elem##_ops = {                             \
                .protocol        = ENUMERIZE(elem),             \
                .connect         = elem##_connect,              \
                .download        = elem##_download,             \
                .upload          = elem##_upload,               \
                .disconnect      = elem##_disconnect,           \
                .get_xattr_value = elem##_get_xattr_value,      \
                .get_xattr_size  = elem##_get_xattr_size,       \
        }

typedef struct {
        enum protocol_enum protocol;
        int    (*connect)(void);
        int    (*download)(const char *path);
        int    (*upload)(const char *path);
        void   (*disconnect)(void);
        char  *(*get_xattr_value)(const char *path);
        size_t (*get_xattr_size)(void);
} ops_t;

int    s3_connect(void);
int    s3_download(const char *path);
int    s3_upload(const char *path);
void   s3_disconnect(void);
char  *s3_get_xattr_value(const char *path);
size_t s3_get_xattr_size(void);

int download_file(const char *path);
int upload_file(const char *path);
int is_file_local(const char *path);
int is_valid_path(const char *path);


#include <stddef.h>
#include <time.h>

typedef struct {
        /* 4096 is minimum acceptable value (including null) according to POSIX (equals PATH_MAX from linux/limits.h) */
        char   fs_mount_point[4096];          /* filesystem's root directory */

        time_t scanfs_iter_tm_sec;            /* the lowest time interval between file system scan iterations */

        double move_out_start_rate;           /* start evicting files when storage is (move_out_start_rate * 100)% full */
        double move_out_stop_rate;            /* stop evicting files when storage is (move_out_stop_rate * 100)% full */

        size_t primary_download_queue_max_size;
        size_t secondary_download_queue_max_size;

        size_t primary_upload_queue_max_size;
        size_t secondary_upload_queue_max_size;

        size_t path_max;                      /* maximum path length in fs_mount_point directory can not be lower than this value */

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

        /* maximum number of retries of s3 requests */
        int s3_operation_retries;
} conf_t;

int read_conf(const char *conf_path);
conf_t *get_conf();
log_t  *get_log();
ops_t  *get_ops();


/*
 * SCANFS
 */

int scan_fs(queue_t *download_queue, queue_t *upload_queue);


#endif /* CLOUDTIERING_H */
