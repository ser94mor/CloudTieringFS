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
#include "file.h"

/* buffer to store error messages (mostly errno messages) */
static __thread char err_buf[ERR_MSG_BUF_LEN];

/* declare array of extended attributes' keys */
static const char *xattr_str[] = {
        XATTRS(XATTR_KEY, COMMA),
};

/**
 * @brief close_handle_err Close file descriptor and handle errors, if any.
 *
 * @param[in] fd        File descriptor to be closed.
 * @param[in] path      Path to the file which file descriptor should be closed.
 * @param[in] func_name Name of the function which called this close wrapper.
 */
static void close_handle_err( int fd,
                              const char *path,
                              const char *func_name ) {
        if ( close( fd ) == -1 ) {
                /* TODO: consider to handle EINTR */

                /* strerror_r() with very low probability can fail;
                   ignore such failures */
                strerror_r( errno, err_buf, ERR_MSG_BUF_LEN );

                LOG( ERROR,
                     "[%s] failed to close file descriptor "
                     "[ path: %s | fd: %d | reason: %s ]",
                     func_name,
                     path,
                     fd,
                     err_buf );
        }
}

/**
 * Perform file upload operation from local storage to remote storage.
 * See ops.h for complete description.
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

                close_handle_err( fd, path, "upload_file" );

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

                close_handle_err( fd, path, "upload_file" );

                return 0;
        }

        /* calculate object id (the key) for the remote object storage */
        const char *object_id     = get_ops()->get_object_id_xattr_value( path );
        size_t object_id_max_size = get_ops()->get_object_id_xattr_size();

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

                close_handle_err( fd, path, "upload_file" );

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

                close_handle_err( fd, path, "upload_file" );

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

                close_handle_err( fd, path, "upload_file" );

                return -1;
        }

        /* TODO: check that file still meets eviction requirements */
        /* get stat structure to determine the file size */
        struct stat stat_buf;
        if ( fstat( fd , &stat_buf ) == -1) {
                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[upload_file] aborting file upload operation because "
                     "failed to fstat() file "
                     "[ fd: %d | reason: %s ]",
                     fd,
                     err_buf );

                /* NOTE: failues in the cleanup functions are impossible
                         as long as the program's logic is correct */
                unlock_file( fd );

                close_handle_err( fd, path, "upload_file" );

                return -1;
        }

        /* truncate file to length 0 qq*/
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

                close_handle_err( fd, path, "upload_file" );

                return -1;
        }

        /* FIXME: here we have a risk that process that calls stat() from
                  another process will get st_size == 0;
                  on single-node file systems we can use fallocate() call with
                  FALLOC_FL_PUNCH_HOLE mode to transform file to
                  sparce via single system call */

        /* make the file sparse by extending it to the original length */
        if ( ftruncate( fd, stat_buf.st_size ) == -1 ) {
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

                close_handle_err( fd, path, "upload_file" );

                return -1;
        }

        /* all actions indended to file upload succeeded; just need to unlock */
        /* NOTE: failues in the cleanup functions are impossible
                 as long as the program's logic is correct */
        unlock_file( fd );

        close_handle_err( fd, path, "upload_file" );

        return 0;
}

/**
 * Perform file download operation from remote storage to local storage.
 * See ops.h for complete description.
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

                close_handle_err( fd, path, "download_file" );

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

                close_handle_err( fd, path, "download_file" );

                return 0;
        }

        size_t object_id_max_size = get_ops()->get_object_id_xattr_size();
        char object_id[object_id_max_size];

        if ( get_xattr( fd,
                        e_object_id,
                        object_id,
                        object_id_max_size ) == -1 ) {
                LOG( ERROR,
                     "[download_file] aborting file %s download operation "
                     "because failed to obtain value of %s extended attribute",
                     xattr_str[e_object_id],
                     path );

                unlock_file( fd );

                close_handle_err( fd, path, "download_file" );

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

                close_handle_err( fd, path, "download_file" );

                return -1;
        }

        /* FIXME: need to remove all extended attributes or repair attributes
                  that has already been removed */

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

                close_handle_err( fd, path, "download_file" );

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

                close_handle_err( fd, path, "download_file" );

                return -1;
        }

        /* NOTE: failues in the cleanup functions are impossible
                 as long as the program's logic is correct */
        unlock_file( fd );

        close_handle_err( fd, path, "download_file" );

        return 0;
}
