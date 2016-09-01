#ifndef CONF_H
#define CONF_H

#define ORANGEFS_CLIENT_ROOT_DIRECTORY "/mnt/orangefs/"
#define TIER_COLD_HIDDEN_DIR ".tier-cold/"
#define ORANGEFS_CLIENT_ROOT_DIRECTORY_MODE (mode_t)777

#define EVICT_SESSION_TIMEOUT (time_t)60 /* the lowest time interval between evict sessions */
#define EVICT_SESSION_SLEEP (time_t)30

#define START_EVICT_SPACE_RATE_LIMIT 0.7 /* start evicting files when storage is 70% full (30% empty) */
#define STOP_EVICT_SPACE_RATE_LIMIT  0.6 /* stop evicting files when storage is 60% full (40% empty) */

#define EVICT_QUEUE_MAX_SIZE 100000

#define ORANFEFS_TIER_TYPE 0
#define S3_TIER_TYPE       1

struct __conf {
    char *fs_root_dir;         /* filesystem's root directory */
    time_t ev_session_tm;      /* the lowest time interval between evict sessions */
    double start_ev_rate;      /* start evicting files when storage is (start_ev_rate * 100)% full */
    double stop_ev_rate;       /* stop evicting files when storage is (stop_ev_rate * 100)% full */
    size_t ev_q_max_size;      /* maximum size of evict queue */
    unsigned int log_level;    /* current log level */
    unsigned int tier_type[2]; /* array of tier types */

    char *cold_tier_dir;        /* temporary */
    mode_t cold_tier_dir_mode;  /* temporary */
    time_t evict_session_sleep; /* temporary */
};

typedef struct __conf conf_t;

const conf_t conf;

#endif // CONF_H

