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

#define _POSIX_C_SOURCE    200112L    /* required for strerror_r() */
#define _XOPEN_SOURCE 500    /* needed to use nftw() */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libs3.h>

#include "cloudtiering.h"

static __thread char err_buf[ERR_MSG_BUF_LEN];

/* declare array of extended attributes' keys */
static const char *xattr_str[] = {
        XATTRS(XATTR_KEY, COMMA),
};

/* declare array of extended attributes' sizes */
static size_t xattr_max_size[] = {
        XATTRS(XATTR_MAX_SIZE, COMMA),
};

/* declare types of extended attributes' values */
XATTRS(DECLARE_XATTR_TYPE, SEMICOLON);

static int is_file_locked(const char *path) {
        int ret = getxattr(path, xattr_str[e_locked], NULL, 0);

        if (ret == -1) {
                if (errno == ENOATTR) {
                        return 0; /* false; not locked */
                }

                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to get extended attribute %s of file %s [reason: %s]",
                    xattr_str[e_locked],
                    path,
                    err_buf);

                return -1; /* error indication */
        }

        return 1; /* true; file is locked */
}

static int lock_file(const char *path) {
        int ret = setxattr(path, xattr_str[e_locked], NULL, 0, XATTR_CREATE);

        if (ret == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s to file %s [reason: %s]",
                    xattr_str[e_locked],
                    path,
                    err_buf);

                return -1;
        }

        return 0;
}

static int unlock_file(const char *path) {
        int ret = removexattr(path, xattr_str[e_locked]);

        if (ret == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to remove extended attribute %s of file %s [reason: %s]",
                    xattr_str[e_locked],
                    path,
                    err_buf);

                return -1;
        }

        return 0;
}

/**
 * @brief is_file_local Check whether file data is on local store or on remote.
 * @param[in] path Path to target file.
 * @return 0 is file data on remote store and >0 when file data on local store; -1 on error
 */
static int is_file_local(const char *path) {
        size_t size = xattr_max_size[e_location];
        const char *xattr = xattr_str[e_location];
        xattr_location_t loc = 0;

        if (getxattr(path, xattr, &loc, size) == -1) {
                if (errno == ENOATTR) {
                        return 1; /* true */
                }

                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to get extended attribute %s of file %s [reason: %s]",
                    xattr,
                    path,
                    err_buf);

                return -1; /* error indication */
        }

        return !!(loc == LOCAL_STORE);
}

/**
 * @brief is_valid_path Checks if path is valid - exists and is is regular file.
 * @param[in] path Path to target file.
 * @return 0 if path is not valid and non-0 if path is valid.
 */
static int is_valid_path(const char *path) {
        struct stat path_stat;
        if ((stat(path, &path_stat) == -1) || !S_ISREG(path_stat.st_mode)) {
                return 0; /* false */
        }

        return 1; /* true */
}

int move_file_out(const char *path) {
        if (lock_file(path) == -1) {
                return -1;
        }

        if (get_ops()->upload(path) == -1) {
                LOG(ERROR, "failed to put object into remote store for file %s", path);
                /* TODO: unlock_file() in all error places */
                return -1;
        }
        LOG(DEBUG, "file %s was uploaded to remote store successfully", path);

        const char *key = xattr_str[e_location];
        xattr_location_t location_val = REMOTE_STORE;

        if (setxattr(path, key, &location_val, xattr_max_size[e_location], XATTR_CREATE) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s with value %s to file %s [reason: %s]",
                    key,
                    path,
                    path,
                    err_buf);

                return -1;
        }

        int ret = 0;
        int fd;
        if ((fd = open(path, O_TRUNC | O_WRONLY)) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to execute open() to truncate file %s [reason: %s]",
                    path,
                    err_buf);

                ret = -1;
        }

        if (close(fd) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to execute close() on file %s [reason: %s]",
                    path,
                    err_buf);

                ret = -1;
        }

        if (unlock_file(path) == -1) {
                ret = -1;
        }

        return ret;
}

int move_file_in(const char *path) {
        /* TODO: need to implement this function */
        return -1;
}

/**
 * @brief move_file Move file on the front of the queue in or out (obvious from the file attributes).
 * @param[in] queue Queue with candidate files for moving in or out.
 * @return -1 on failure; 0 on success
 */
int move_file(queue_t *queue) {
        if (!queue_empty(queue)) {
                /* double queue_front invocation does not lead to violation of
                   thread-safeness since given queue has only one consumer
                   (this thread) */

                /* get item size of front element */
                size_t it_sz = 0;
                queue_front(queue, NULL, &it_sz);

                /* get front element of the queue */
                char path[it_sz];
                queue_front(queue, path, &it_sz);

                if (path == NULL) {
                        strcpy(err_buf, "failed to get front element of queue");
                        goto err;
                }

                if (!is_valid_path(path)) {
                        sprintf(err_buf, "path %s is not valid", path);
                        goto err;
                }

                int local_flag = 0;
                if (is_file_local(path)) {
                        local_flag = 1;

                        /* local files should be moved out */
                        if (move_file_out(path) == -1) {
                                sprintf(err_buf, "failed to move file %s out", path);
                                goto err;
                        }
                } else {
                        local_flag = 0;

                        /* remote files should be moved in */
                        if (move_file_in(path) == -1) {
                                sprintf(err_buf, "failed to move file %s in", path);
                                goto err;
                        }
                }

                queue_pop(queue);
                LOG(INFO, "file %s successfully moved %s", path, (local_flag ? "in" : "out"));
        } else {
                //LOG(DEBUG, "queue is empty; nothing to move");
        }

        /* return successfully if there are no files to move or move in/out operation was successful */
        return 0;

        err:
        queue_pop(queue);
        LOG(ERROR, "failed to move file [reason: %s]", err_buf);
        return -1;
}

/*******************
 * Scan filesystem *
 * *****************/
/* queue might be full and we need to perform several attempts giving a chance
 * for other threads to pop elements from queue */
#define QUEUE_PUSH_RETRIES              5
#define QUEUE_PUSH_ATTEMPT_SLEEP_SEC    1

static queue_t *in_queue  = NULL;
static queue_t *out_queue = NULL;

static int update_evict_queue(const char *fpath, const struct stat *sb,  int typeflag, struct FTW *ftwbuf) {
        int attempt = 0;

        struct stat path_stat;
        if (stat(fpath, &path_stat) == -1) {
                return 0; /* just continue with the next files; non-zero will cause failure of nftw() */
        }

        if (S_ISREG(path_stat.st_mode) &&
            is_file_local(fpath) &&
            !is_file_locked(fpath) &&
            (path_stat.st_atime + 600) < time(NULL)) {
                do {
                        if (queue_push(out_queue, (char *)fpath, strlen(fpath) + 1) == -1) {
                                ++attempt;
                                continue;
                        }
                        LOG(DEBUG, "file '%s' pushed to out queue", fpath);
                        break;
                } while ((attempt < QUEUE_PUSH_RETRIES) && (queue_full((queue_t *)out_queue) > 0));
        }

        return 0;
}

int scanfs(queue_t *in_q, queue_t *out_q) {
        conf_t *conf = get_conf();

        in_queue  = in_q;
        out_queue = out_q;

        /* set maximum number of open files for nftw to a half of descriptor table size for current process */
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
                return -1;
        }

        /* rlim_cur value will be +1 higher than actual according to documentation */
        if (rlim.rlim_cur <= 2) {
                /* there are 1 or less descriptors available */
                return -1;
        }
        int nopenfd = rlim.rlim_cur / 2;

        /* stay within filesystem and do not follow symlinks */
        int flags = FTW_MOUNT | FTW_PHYS;

        if (nftw(conf->fs_mount_point, update_evict_queue, nopenfd, flags) == -1) {
                return -1;
        }

        return 0;
}
