#ifndef CLOUDTIERING_H
#define CLOUDTIERING_H


/*
 * LOG
 */

typedef struct {
        void (*open_log)(const char *);
        void (*log)(int, const char *, ...);
        void (*close_log)(void);
} log_t;

#include <syslog.h>

#define ERROR      LOG_ERR
#define INFO       LOG_INFO
#define DEBUG      LOG_DEBUG

/* logger operations are wrapped by macroses to be able painlessly change logger type in the future */

#define OPEN_LOG(name) { \
            openlog(name, LOG_PID, LOG_DAEMON); \
        }

#define LOG(level,format,args...) { \
            syslog(level, format, ## args); \
        }

#define CLOSE_LOG() { \
            closelog(); \
        }


/*
 * CONF
 */

#include <stddef.h>
#include <time.h>

typedef struct {
    char   fs_mount_point[4096];          /* filesystem's root directory */
    time_t scanfs_iter_tm_sec;            /* the lowest time interval between file system scan iterations */
    double ev_start_rate;                 /* start evicting files when storage is (start_ev_rate * 100)% full */
    double ev_stop_rate;                  /* stop evicting files when storage is (stop_ev_rate * 100)% full */
    size_t ev_q_max_size;                 /* maximum size of evict queue */
    size_t path_max;                      /* maximum path length in fs_mount_point directory can not be lower than this value */
    log_t  logger;                        /* implementation neutral logger */
} conf_t;

int readconf(const char *conf_path);
conf_t *getconf();


/*
 * QUEUE
 */

#include <stdio.h>
#include <sys/types.h>

typedef struct {
    char *head;
    char *tail;
    size_t cur_q_size;
    size_t max_q_size;
    size_t max_item_size;
    char *buffer;
    size_t buffer_size;
} queue_t;

int queue_empty(queue_t *q);
int queue_full(queue_t *q);

int queue_push(queue_t *q, char *item, size_t item_size);
int queue_pop(queue_t *q);

char *queue_front(queue_t *q, size_t *size);

queue_t *queue_alloc(size_t max_q_size, size_t max_item_size);
void queue_free(queue_t *q);

void queue_print(FILE *stream, queue_t *q);


/*
 * SCANFS
 */

int scanfs(const queue_t *queue);

#endif // CLOUDTIERING_H

