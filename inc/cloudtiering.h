#ifndef CLOUDTIERING_H
#define CLOUDTIERING_H


/*
 * LOG
 */

typedef struct {
        char name[256];                         /* human-readable name of logging framework */
        void (*open_log)(const char *);         /* calls open log function from logger framework */
        void (*log)(int, const char *, ...);    /* calls log function from logger framework */
        void (*close_log)(void);                /* calls close log fucntion from logger framework */
        int  error;                             /* integer representing ERROR level in logging framework */
        int  info;                              /* integer representing INFO level in logging framework */
        int  debug;                             /* integer representing DEBUG level in logging framework */
} log_t;

#define ERROR      (getconf()->logger.error)
#define INFO       (getconf()->logger.info)
#define DEBUG      (getconf()->logger.debug)

#define OPEN_LOG(name)            (getconf()->logger.open_log(name))
#define LOG(level,format,args...) (getconf()->logger.log(level, format, ## args))
#define CLOSE_LOG()               (getconf()->logger.close_log())


/*
 * QUEUE
 */

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>

typedef struct {
        char  *head;
        char  *tail;
        size_t cur_q_size;
        size_t max_q_size;
        size_t max_item_size;
        char  *buffer;
        size_t buffer_size;
        pthread_mutex_t mutex;
} queue_t;

int queue_empty(queue_t *q);
int queue_full(queue_t *q);

int queue_push(queue_t *q, char *item, size_t item_size);
int queue_pop(queue_t *q);

char *queue_front(queue_t *q, size_t *size);

queue_t *queue_alloc(size_t max_q_size, size_t max_item_size);
void queue_free(queue_t *q);


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
        log_t  logger;                        /* implementation neutral logger */
        ops_t  ops;                           /* operation (depends on remote store protocol) */

        /* 63 it is unlikely that protocol identifies require more symbols */
        char   remote_store_protocol[64];

        /* 255 equals to S3_MAX_HOSTNAME_SIZE from libs3 */
        char   s3_default_hostname[256];      /* s3 default hostname (libs3) */

        /* 63 is maximum allowed bucket name according to http://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html */
        char   s3_bucket[64];                 /* name of s3 bucket which will be used as a remote tier */

        /* 32 is maximum access key id length according to http://docs.aws.amazon.com/IAM/latest/APIReference/API_AccessKey.html */
        char   s3_access_key_id[33];          /* s3 access key id */

        /* 127 is an assumption that in practice longer keys do not exist */
        char   s3_secret_access_key[128];     /* s3 secret access key */
} conf_t;

int readconf(const char *conf_path);
conf_t *getconf();


/*
 * SCANFS
 */

int scanfs(queue_t *in_q, queue_t *out_q);

#endif /* CLOUDTIERING_H */

