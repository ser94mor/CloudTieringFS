#ifndef CONF_H
#define CONF_H

#include <stddef.h>
#include <time.h>
#include <linux/limits.h>

typedef struct {
    char   fs_mount_point[PATH_MAX];      /* filesystem's root directory */
    time_t ev_session_tm;                 /* the lowest time interval between evict sessions */
    double ev_start_rate;                 /* start evicting files when storage is (start_ev_rate * 100)% full */
    double ev_stop_rate;                  /* stop evicting files when storage is (stop_ev_rate * 100)% full */
    size_t ev_q_max_size;                 /* maximum size of evict queue */
} conf_t;

int readconf(const char *conf_path);
conf_t *getconf();

#endif // CONF_H

