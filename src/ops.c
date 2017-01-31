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
#define _XOPEN_SOURCE      500        /* required for truncate() */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cloudtiering.h"


/* buffer to store error messages (mostly errno messages) */
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

/**
 * @brief lock_file Lock file using extended attribute as an indicator.
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] path Path of file to set lock.
 *
 * @return  0: file was locked
 *         -1: file was not locked due to failure or lock was already set on
 *             this file previously
 */
static int lock_file(const char *path) {
        int ret = setxattr(path, xattr_str[e_locked], NULL, 0, XATTR_CREATE);

        if (ret == -1) {
                if (errno == EEXIST) {
                        /* file is already locked; this is not a system error
                           but a caller of this function should not proceed
                           futher with his actions */
                        LOG(DEBUG,
                            "unable to lock file %s; it is already locked",
                            path);

                        return -1;
                }

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s to file %s "
                    "[reason: %s]",
                    xattr_str[e_locked],
                    path,
                    err_buf);

                return -1;
        }

        return 0;
}

/**
 * @brief unlock_file Unlock file via removal of extended attribute.
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] path Path of file to unset lock.
 *
 * @return  0: file was unlocked
 *         -1: file was not unlocked due to failure
 */
static int unlock_file(const char *path) {
        int ret = removexattr(path, xattr_str[e_locked]);

        if (ret == -1) {
                /* non-existance of lock indicates an error in logic
                   of this program */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to remove extended attribute %s of file %s "
                    "[reason: %s]",
                    xattr_str[e_locked],
                    path,
                    err_buf);

                return -1;
        }

        return 0;
}

/**
 * @brief is_file_local Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] path Path to file to check location.
 *
 * @return  1: if file is in local storage
 *          0: if file is in remote storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_file_local(const char *path) {
        size_t size = xattr_max_size[e_location];
        const char *xattr = xattr_str[e_location];
        xattr_location_t loc = 0;

        if (getxattr(path, xattr, &loc, size) == -1) {
                if (errno == ENOATTR) {
                        return 1; /* true */
                }

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to get extended attribute %s of file %s "
                    "[reason: %s]",
                    xattr,
                    path,
                    err_buf);

                return -1; /* error indication */
        }

        return !!(loc == LOCAL_STORAGE);
}

/**
 * @brief is_valid_path Checks if path is valid, i.e. exists and
 *                      file is regular.
 *
 * @param[in] path Path to file to validate.
 *
 * @return 1: if path is valid
 *         0: if path is not valid
 */
static int is_valid_path(const char *path) {
        struct stat path_stat;
        if ((stat(path, &path_stat) == -1) || !S_ISREG(path_stat.st_mode)) {
                return 0; /* false */
        }

        return 1; /* true */
}

/**
 * @brief upload_file Upload file to remote storage from local storage.
 *
 * @param[in] path Path to a file to upload.
 *
 * @return  0: file has been upload to remote storage properly and truncated
 *         -1: file has not been upload due to error or access to file
 *             has happen during eviction
 */
static int upload_file(const char *path) {
        /* set lock to file to prevent other threads' and processes'
           access to file's data */
        if (lock_file(path) == -1) {
                return -1;
        }

        /* upload file's data to remote storage */
        if (get_ops()->upload(path) == -1) {
                goto err_unlock; /* unset lock and exit */
        }

        /* set file's location information */
        const char *key = xattr_str[e_location];
        xattr_location_t location_val = REMOTE_STORAGE;
        if (setxattr(path, key, &location_val, xattr_max_size[e_location],
                     XATTR_CREATE) == -1) {
                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s with value %s to "
                    "file %s [reason: %s]",
                    key,
                    path,
                    path,
                    err_buf);

                goto err_unlock; /* unset lock and exit */
        }

        /* truncate file */
        if (truncate(path, 0) == -1) {
                /* TODO: handle EINTR case */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to truncate file %s [reason: %s]",
                    path,
                    err_buf);

                /* remove location attribute, unset lock and exit */
                goto err_location_unlock;
        }

        /* all actions indended to file upload succeeded; just need to unlock */
        if (unlock_file(path) == -1) {
                /* TODO: do some repair actions;
                         files should not be left locked */
                return -1;
        }

        return 0;

        err_location_unlock:
        /* remove location attribute */
        if (removexattr(path, xattr_str[e_location]) == -1) {
                /* non-existance of location attribute indicates an error
                   in logic of this program */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to remove extended attribute %s of file %s "
                    "[reason: %s]",
                    xattr_str[e_location],
                    path,
                    err_buf);
        }

        err_unlock:
        /* unset lock from file */
        if (unlock_file(path) == -1) {
                /* TODO: do some repair actions;
                         files should not be left locked */
        }

        return -1;
}

/**
 * @brief dowload_file Download file from remote storage to local storage.
 *
 * @param[in] path Path to file to download.
 *
 * @return  0: file has been successfully dowloaded
 *         -1: failure happen due dowload of file or
 *             file is currently being downloaded by another thread
 */
static int dowload_file(const char *path) {
        /* TODO: need to implement this function */
        return -1;
}

/**
 * @brief move_file Download or upload file in the front of the queue.
 *                  Move direction is obvious from file attributes.
 *
 * @param[in,out] queue Queue with candidate files for download or upload.
 *
 * @return  0: download or upload operation succeeded
 *         -1: failure happen and download or upload was not successful
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

                int local_flag = is_file_local(path);
                if (local_flag == -1) {
                        /* do nothing if unable to determine file's location */
                        sprintf(err_buf,
                                "unable to determine file's location "
                                "[file: %s]",
                                path);
                        goto err;
                } else if (local_flag) {
                        /* local files should be uploaded */
                        if (upload_file(path) == -1) {
                                sprintf(err_buf,
                                        "upload of file %s failed",
                                        path);
                                goto err;
                        }
                } else {
                        /* remote files should be downloaded */
                        if (dowload_file(path) == -1) {
                                sprintf(err_buf,
                                        "download of file %s failed",
                                        path);
                                goto err;
                        }
                }

                queue_pop(queue);
                LOG(INFO,
                    "file %s successfully %s",
                    path,
                    (local_flag ? "uploaded" : "downloaded"));
        } else {
                /* queue is empty; nothing to be done */
        }

        /* return successfully if there are no files to download/upload or
           download/upload operation was successful */
        return 0;

        err:
        queue_pop(queue);
        LOG(ERROR, "failed to move file [reason: %s]", err_buf);
        return -1;
}
