#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/errno.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdarg.h>

#include "evict-queue.h"
#include "memory.h"
#include "log.h"
#include "config.h"
#include "tier-engine.h"

/**
 * Returns non-zero if file is regular and zero otherwise.
 */
int is_regular_file(const char *dir_name, const char *file_name) {
    char *path = calloc(strlen(dir_name) +
                        strlen(file_name) +
                        1 /* '/' character */ +
                        1 /* 0-code character */, sizeof(char));
    strcat(path, dir_name);
    strcat(path, "/");
    strcat(path, file_name);

    struct stat path_stat;
    stat(path, &path_stat);

    free(path);
    return S_ISREG(path_stat.st_mode);
}

/**
 * Returns non-zero if file is regular and zero otherwise.
 */
time_t file_access_time(const char *dir_name, const char *file_name) {
    char *path = calloc(strlen(dir_name) +
                        strlen(file_name) +
                        1 /* '/' character */ +
                        1 /* 0-code character */, sizeof(char));
    strcat(path, dir_name);
    strcat(path, "/");
    strcat(path, file_name);

    struct stat path_stat;
    stat(path, &path_stat);

    free(path);
    return path_stat.st_atime;
}

/**
 * Creates cold tier directory or exit silently (no return value).
 */
void mkdir_tier_cold(char *cl_dir_path, int mode) {

    /* constructing full directory path */
    unsigned int tier_cold_dir_len = strlen(cl_dir_path) + strlen(TIER_COLD_HIDDEN_DIR);
    char *tier_cold_dir = (char *)malloc(tier_cold_dir_len + 1);
    memset(tier_cold_dir, 0, tier_cold_dir_len + 1);
    strcat(strcat(tier_cold_dir, cl_dir_path), TIER_COLD_HIDDEN_DIR);

    /* create hidded cold tier directory if not created already */
    struct stat st = { 0 };
    if (stat(tier_cold_dir, &st) == -1) {
        int rc = mkdir(tier_cold_dir, mode);
        if (!rc) {
            /* no way to continue execution if directory could not be created */
            exit(rc);
        }
    }
}

void update_file_list(const char *dir_path, const char *file_list[], size_t file_list_size) {
    DIR *dp;
    struct dirent *ep;

    dp = opendir(dir_path);
    if (dp != NULL) {

        size_t file_cnt = 0;
        while ( (ep = readdir(dp)) ) {
            /* if regular file push it into evict_queue */
            if (is_regular_file(dir_path, ep->d_name)) {
                file_list[file_cnt] = calloc(256, sizeof(char));
                strcpy((char *)file_list[file_cnt], ep->d_name);
                file_cnt++;
            }
        }
        (void) closedir(dp);

    } else {
        char msg[256] = "Couldn't open the directory ";
        strcat(msg, dir_path);

        log(ERROR, msg);
    }
}

void update_evict_queue(ev_q *evict_queue) {
    log(INFO, "updating evict queue...");

    /* get list of files in root orangefs directory */
    const char *file_list[evict_queue->max_size];
    for (int i = 0; i < evict_queue->max_size; i++) {
        file_list[i] = NULL;
    }

    update_file_list(ORANGEFS_CLIENT_ROOT_DIRECTORY, file_list, evict_queue->max_size);

    int i = 0;
    while (file_list[i] != NULL && i < evict_queue->max_size) {

        time_t now = time (NULL); /* get system time */
        if (now >= file_access_time(ORANGEFS_CLIENT_ROOT_DIRECTORY, file_list[i]) + EVICT_SESSION_TIMEOUT) {
            ev_q_push(evict_queue, ev_n_alloc(ORANGEFS_CLIENT_ROOT_DIRECTORY, file_list[i]));
        }

        i++;
    }

}

void move_file_to_tier_cold(const char *hot_path, const char *cold_path) {
    log(DEBUG, "|HOT -> COLD| : { %s -> %s }", hot_path, cold_path);

    rename(hot_path, cold_path);
}

void move_file_to_tier_hot(const char *cold_path, const char *hot_path) {
    log(DEBUG, "|COLD -> HOT| : { %s -> %s }", cold_path, hot_path);
    rename(cold_path, hot_path);
}

void try_evict_files(ev_q *evict_queue) {
    while (evict_queue->size > 0) {
        ev_n *node = ev_q_front(evict_queue);

        int old_buf_size = strlen(node->dir_name) + strlen(node->file_name) + 1;
        char old_buf[old_buf_size];
        memset(old_buf, 0, old_buf_size);
        strcat(old_buf, node->dir_name);
        strcat(old_buf, node->file_name);

        int new_buf_size = strlen(node->dir_name) + strlen(node->file_name) + strlen(TIER_COLD_HIDDEN_DIR) + 1;
        char new_buf[new_buf_size];
        memset(new_buf, 0, new_buf_size);
        strcat(new_buf, node->dir_name);
        strcat(new_buf, TIER_COLD_HIDDEN_DIR);
        strcat(new_buf, node->file_name);

        time_t now = time(NULL); /* get system time */
        if (now >= file_access_time(node->dir_name, node->file_name) + EVICT_SESSION_TIMEOUT) {
            move_file_to_tier_cold(old_buf, new_buf);
        }

        ev_q_pop(evict_queue);
    }
}

int pull_file_data(const char *path) {
    return 0;
}

int is_dummy(const char *path) {
    ssize_t dummy_attr_size = getxattr(path, "is_dummy", NULL, 0);

    if (dummy_attr_size > 0) {
      return 1;
    } else if (dummy_attr_size < 0) {
      log(ERROR, "error getting 'is_dummy' attribute for '%s' file (errno: %d)", path, errno);
    }

    return 0;
}

int load_file_data(const char *path) {
    if (is_dummy(path)) {
        /* file is dummy; need to pull its data from the cold tier */
        char cold_id[1024]; /* TODO: consider changing array size to something else */
        memset(cold_id, 0, sizeof(cold_id));

	/* when 'size' parameter set to 0 for getxattr, size in bytes for attribute value is returned */
	ssize_t cold_id_size = getxattr(path, "cold_id", NULL, 0);

        if (getxattr(path, "cold_id", &cold_id, cold_id_size)) {
            log(ERROR, "error getting 'cold_id' attribute for '%s' file (errno: %d)", path, errno);
            return errno;
        }
        log(DEBUG, "FILE '%s' | XATTR 'cold_id':'%s'", path, cold_id);

        /* actually pull file data from cold tier */
        if (pull_file_data(cold_id)) {
            log(ERROR, "error pulling file data from cold tier (errno: %d)", rc, errno);
            return errno;
        }
    } else {
        /* file is not dummy; nothing to do */
    }

    return 0;
}

/**
 * Returns value from interval [0, 1] indicating rate of space occupation.
 */
double occupied_space_rate() {
    /* TODO: implement this method */
    return 0.8;
}

/**
 * Updates config_t structure with new values. Returns non-zero in case of failure.
 */
int update_config(config_t *config) {
    /* TODO: implement reading configuration from file */

    config->fs_root_dir = ORANGEFS_CLIENT_ROOT_DIRECTORY;
    config->ev_session_tm = EVICT_SESSION_TIMEOUT;
    config->start_ev_rate = START_EVICT_SPACE_RATE_LIMIT;
    config->stop_ev_rate = STOP_EVICT_SPACE_RATE_LIMIT;
    config->ev_q_max_size = EVICT_QUEUE_MAX_SIZE;
    config->log_level = DEBUG;
    config->tier_type[0] = ORANFEFS_TIER_TYPE;
    config->tier_type[1] = S3_TIER_TYPE;


    config->cold_tier_dir = TIER_COLD_HIDDEN_DIR;        /* temporary */
    config->cold_tier_dir_mode = ORANGEFS_CLIENT_ROOT_DIRECTORY_MODE;  /* temporary */
    config->evict_session_sleep = EVICT_SESSION_SLEEP; /* temporary */

    return 0;
}

/**
 * Performs preparational actions required to successfully start tier engine.
 * In case of success returns 0, in case of failure returns (!= 0)
 */
int prepare(config_t *config) {
    if (update_config(config)) {
        log(ERROR, "unable to update configuration; terminate");
        return 1;
    }

    mkdir_tier_cold(config->fs_root_dir, config->cold_tier_dir_mode);
    return 0;
}

int main(int argc, char *argv[]) {
    log(INFO, "start tier-engine");

    config_t config;

     /* make some preparational actioins and checks */
    if (prepare(&config)) {
        return EXIT_FAILURE;
    }
    ev_q *evict_queue = ev_q_alloc(config.ev_q_max_size);; /* queue of regular files (eviction candidates) */

    double occ_sp_rt = 0.0; /* default: 0% storage occupied */
    unsigned int evict_active = 0; /* default: eviction is inactive */
    while (1) {
        const time_t start_time = time(NULL);
        log(DEBUG, "start next evict session");

        occ_sp_rt = occupied_space_rate();
        evict_active = (evict_active && occ_sp_rt >= config.stop_ev_rate) ||
                       (!evict_active && occ_sp_rt >= config.start_ev_rate);

        log(DEBUG, "eviction is %sactive; occupied space rate: %2.2f%%", (evict_active ? "" : "in"), occ_sp_rt * 100);
        if (evict_active) {
            update_evict_queue(evict_queue);
            try_evict_files(evict_queue);
        }

        /* calculating time to sleep before next evict session */
        const time_t stop_time = time(NULL);
        const double diff_time = difftime(config.ev_session_tm, (time_t)difftime(stop_time, start_time));
        const double sleep_time = diff_time >= 0.0 ? diff_time : 0.0;
        log(DEBUG, "evict session finished; sleep %.f seconds", sleep_time);
        sleep(sleep_time);
    }

    return EXIT_SUCCESS;
}



