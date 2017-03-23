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

#include "ops.h"
#include "log.h"

/* enum of supported extended attributes */
enum xattr_enum {
        XATTRS(ENUMERIZE, COMMA),
};

/* buffer to store error messages (mostly errno messages) */
static __thread char err_buf[ERR_MSG_BUF_LEN];

/* declare array of extended attributes' keys */
static const char *xattr_str[] = {
        XATTRS(XATTR_KEY, COMMA),
};

static int set_xattr( int fd,
                      enum xattr_enum xattr,
                      void *value,
                      size_t value_size,
                      int flags ) {

        if ( fsetxattr( fd,
                        xattr_str[xattr],
                        value,
                        value_size,
                        flags ) == -1 ) {

                /* strerror_r() with very low probability can fail;
                 *          ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "failed to set extended attribute %s to file"
                     "[fd: %d; reason: %s]",
                     xattr_str[e_locked],
                     fd,
                     err_buf );

                return -1;
        }

        return 0;
}

static int get_xattr_tunable( int fd,
                              enum xattr_enum xattr,
                              void  *value,
                              size_t value_size,
                              int ignore_enoattr ) {
        if ( fgetxattr( fd, xattr_str[xattr], value, value_size ) == -1 ) {
                if ( ignore_enoattr && (errno == ENOATTR) ) {
                        return 1; /* no error, but value is special */
                }

                /* strerror_r() with very low probability can fail;
                 *          ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "failed to get extended attribute %s of file"
                     "[fd: %d; reason: %s]",
                     xattr_str[xattr],
                     fd,
                     err_buf );

                return -1; /* error indication */
        }

        return 0;
}

static int remove_xattr_tunable( int fd,
                                 enum xattr_enum xattr,
                                 int ignore_enoattr ) {

        if ( fremovexattr( fd, xattr_str[xattr] ) == -1 ) {
                if (  ignore_enoattr && ( errno == ENOATTR ) ) {
                        return 0;
                }

                /* strerror_r() with very low probability can fail;
                 *          ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "failed to remove extended attribute %s of file"
                     "[fd: %d; reason: %s]",
                     xattr_str[xattr],
                     fd,
                     err_buf );

                return -1;
        }

        return 0;
}

static inline int remove_xattr( int fd, enum xattr_enum xattr ) {
        return remove_xattr_tunable( fd, xattr, 0 );
}

/**
 * @brief try_lock_file Lock file using extended attribute as an indicator.
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to set lock.
 *
 * @return  0: file was locked
 *         -1: file was not locked due to failure or lock was already set on
 *             this file previously
 */
static int try_lock_file( int fd ) {
        if ( set_xattr( fd, e_locked, NULL, 0, XATTR_CREATE ) == -1 ) {
                /* lock file failures is normal, since another thread or
                   procces may already holding a lock */
                LOG( DEBUG, "failed to lock file [fd: %d]", fd );
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
 * @param[in] fd File descriptor of file to unset lock.
 *
 * @return  0: file was unlocked
 *         -1: file was not unlocked due to failure
 */
static int unlock_file( int fd ) {
        if ( remove_xattr( fd, e_locked ) == -1 ) {
                /* NOTE: impossible case in case program's logic is correct */

                LOG( ERROR, "failed to unlock file [fd: %d]", fd );
                return -1;
        }

        return 0;
}

/**
 * @brief is_local_file Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to check location.
 *
 * @return  1: if file is in local storage
 *          0: if file is in remote storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_local_file( int fd ) {
        /* in case stub attribute is not set returns 1,
           if set returns 0, on error returns -1 */
        return get_xattr_tunable( fd, e_stub, NULL, 0, 1 );
}

/**
 * @brief is_remote_file Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to check location.
 *
 * @return  1: if file is in remote storage
 *          0: if file is in local storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_remote_file( int fd ) {
        /* in case stub attribute is not set returns 0,
           if set returns 1, on error returns -1 */
        int ret = is_local_file( fd );
        return ( ret == -1 ) ? -1 : ( ! ret );
}

/**
 * @brief is_regular_file Checks if path is valid, i.e. exists and
 *                        file is regular.
 *
 * @param[in] fd File descriptor of file to validate.
 *
 * @return  1: if path is valid
 *          0: if path is not valid
 *         -1: error happen during fstat() call
 */
int is_regular_file( int fd ) {
        struct stat path_stat;
        if ( ( fstat( fd, &path_stat ) == -1 ) ) {
                return -1;
        }

        return !! ( S_ISREG( path_stat.st_mode ) );
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
int upload_file( const char *path ) {
        /* in order to prevent race conditions and slightly speed up execution
           open the file once and then work with file descriptor */
        int fd = open( path, O_RDWR );
        if ( fd == -1 ) {
                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "[upload_file] unable to open file "
                     "[ path: %s | reason: %s ]",
                     path,
                     err_buf );

                return -1;
        } else {
                LOG( DEBUG,
                     "[upload_file] file opened successfully "
                     "[ path: %s | fd: %d ]",
                     path,
                     fd );
        }

        /* set lock to file to prevent other threads' and processes'
           access to file's data */
        if ( try_lock_file( fd ) == -1 ) {
                LOG( DEBUG,
                     "[upload_file] aborting file upload operation because "
                     "it is locked by another thread or process "
                     "[ path: %s | fd: %d ]",
                     path,
                     fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[upload_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* check file's location */
        if ( ! is_local_file( fd ) ) {
                LOG( DEBUG,
                     "[upload_file] aborting file upload operation because "
                     "it is already in the remote storage "
                     "[ path: %s | fd: %d ]",
                     path,
                     fd );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[upload_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return 0;
        }

        /* calculate object id (the key) for the remote object storage */
        const char *object_id     = get_ops()->get_xattr_value( path );
        size_t object_id_max_size = get_ops()->get_xattr_size();

        /* upload file's data to remote storage */
        if ( get_ops()->upload( fd, object_id ) == -1 ) {
                LOG( ERROR,
                     "[upload_file] aborting file upload operation because "
                     "file's data upload failed [ path: %s | fd: %d ]",
                     path,
                     fd );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[upload_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* set object id to a corresponding attribute */
        if ( set_xattr( fd,
                        e_object_id,
                        (void *)object_id,
                        object_id_max_size,
                        XATTR_CREATE ) == -1 ) {
                LOG( ERROR,
                     "[upload_file] aborting file upload operation because "
                     "failed to set object identifier as a file's meta-data"
                     "[ path: %s | fd: %d ]",
                     path,
                     fd );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[upload_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* set file's location information */
        if ( set_xattr( fd, e_stub, NULL, 0, XATTR_CREATE ) == -1 ) {
                LOG( ERROR,
                     "[upload_file] aborting file upload operation because "
                     "failed to set location information as a file's meta-data"
                     "[ path: %s | fd: %d ]",
                     path,
                     fd );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                remove_xattr( fd, e_object_id );
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[upload_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* TODO: check that file still meets eviction requirements */

        /* truncate file */
        if ( ftruncate( fd, 0 ) == -1 ) {
                /* TODO: handle EINTR case */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "[upload_file] aborting file upload operation because "
                     "failed to truncate this file "
                     "[path: %s | fd: %s | reason: %s]",
                     path,
                     fd,
                     err_buf );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                remove_xattr( fd, e_object_id );
                remove_xattr( fd, e_stub );
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[upload_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* all actions indended to file upload succeeded; just need to unlock */
        /* NOTE: failues in the cleanup functions are impossible
                 as long as the program's logic is correct */
        unlock_file( fd );

        if ( close( fd ) == -1 ) {
                /* TODO: consider to handle EINTR */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "[upload_file] failed to close file descriptor "
                     "[ path: %s | fd: %d | reason: %s ]",
                     path,
                     fd,
                     err_buf );
        }

        return 0;
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
int download_file( const char *path ) {
        /* in order to prevent race conditions and slightly speed up execution
           open the file once and then work with file descriptor */
        int fd = open( path, O_RDWR );
        if ( fd == -1 ) {
                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "[download_file] unable to open file "
                     "[ path: %s | reason: %s ]",
                     path,
                     err_buf );

                return -1;
        } else {
                LOG( DEBUG,
                     "[download_file] file %s opened with %d file descriptor",
                     path,
                     fd );
        }


        /* set lock to file to prevent other threads' and processes'
           access to file */
        if ( try_lock_file( fd ) == -1 ) {
                LOG( DEBUG,
                     "[download_file] aborting file %s download operation "
                     "because it is locked by another thread or process",
                     path );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[download_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* check file's location */
        if ( is_local_file( fd ) ) {
                LOG( DEBUG,
                     "[download_file] aborting file %s download operation "
                     "because it is already in the local storage",
                     path );
                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[download_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return 0;
        }

        size_t object_id_max_size = get_ops()->get_xattr_size();
        char object_id[object_id_max_size];

        if ( get_xattr_tunable( fd,
                                e_object_id,
                                object_id,
                                object_id_max_size,
                                0 ) == -1 ) {
                LOG( ERROR,
                     "[download_file] aborting file %s download operation "
                     "because failed to obtain value of %s extended attribute",
                     xattr_str[e_object_id],
                     path );

                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[download_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* download file's data to local storage */
        if ( get_ops()->download( fd, object_id ) == -1 ) {
                LOG( ERROR,
                     "[download_file] aborting file %s download operation "
                     "because file's data download failed",
                     path );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[download_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* remove file's location information */
        if ( remove_xattr( fd, e_stub ) == -1 ) {
                /* NOTE: impossible case in case program's logic is correct */

                LOG( ERROR,
                     "[download_file] aborting file %s download operation "
                     "because failed to remove location file's meta-data",
                     path );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[download_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        if ( remove_xattr( fd, e_object_id ) == -1 ) {
                /* NOTE: impossible case in case program's logic is correct */

                LOG( ERROR,
                     "[download_file] aborting file %s download operation "
                     "because failed to remove object identifier "
                     "file's meta-data",
                     path );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */

                        /* strerror_r() with very low probability can fail;
                           ignore such failures */
                        strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                        LOG( ERROR,
                             "[download_file] failed to close file descriptor "
                             "[ path: %s | fd: %d | reason: %s ]",
                             path,
                             fd,
                             err_buf );
                }

                return -1;
        }

        /* NOTE: failues in the cleanup functions are impossible
                 as long as the program's logic is correct */
        unlock_file( fd );

        if ( close( fd ) == -1 ) {
                /* TODO: consider to handle EINTR */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "[download_file] failed to close file descriptor "
                     "[ path: %s | fd: %d | reason: %s ]",
                     path,
                     fd,
                     err_buf );
        }

        return 0;
}
