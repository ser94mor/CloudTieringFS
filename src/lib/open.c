/**
 * Copyright (C) 2017  Sergey Morozov <sergey94morozov@gmail.com>
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

#define _GNU_SOURCE        /* needed for O_TMPFILE */

#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "defs.h"
#include "syms.h"

/**
 * Common part for open(2) and openat(2) system call redefinitions.
 */
static inline int finish_open_common( int fd, int flags ) {
        /* we are here when open() call succeeded (i. e. fd != -1);
           we should determine whether file local or remote and if remote,
           schedule its download in daemon */

        int ret = is_local_file( fd, flags );

        /* handle the case when the file is local */
        if ( ret && ( ret != -1 ) ) {
                /* file is local; no need to download it */
                return fd;
        }

        /* handle the case when the file is remote */
        if ( ret == 0 ) {
                /* check O_TRUNC flag and avoid unnessesary download */
                if ( ( ( flags & O_TRUNC ) == O_TRUNC ) ) {
                        /* if O_TRUNC is used with flags other than O_RDWR
                           and O_WRONLY then open() behaviour is unspecified
                           (see open(2)) */

                        /* NOTE: errors in clean_xattr, except ENOATTR are
                                 impossible as long as the programs logic is
                                 correct */
                        if ( clear_xattrs( fd ) == -1 ) {
                                /* can do nothing in case of failure */
                                close( fd );
                                errno = EIO;
                                return -1;
                        }

                        return fd;
                }

                if ( schedule_download( fd ) == -1 ) {
                        /* errno has been set inside that function */
                        return -1;
                }

                if ( poll_file_location( fd, flags, 0 ) == -1 ) {
                        /* errno has been set inside that function */
                        return -1;
                }

                /* by now we do not pass file descriptors to the daemon and
                   that is why we are still on the 0 offset, so just return
                   a file descriptor */
                return fd;
        }

        /* is_file_local() could possibly return errors; we need to map these
           errors to open() call errors to preserve semantics */
        if ( ret == -1 ) {
                /* is_file_local() returned a error, hence stat() errors should
                   be handled here; the only error that does not have analog in
                   open() for our case (when we use file descriptor
                   for fstat()) is EBADF;
                   for nearly impossible situations, such as "during this call
                   another thread called close() with this file descriptor",
                   return EIO error, which is not specified as a possible return
                   values of this call */
                errno = ( errno == EBADFD ) ? EIO : errno;
                return -1;
        }

        /* unreachable place */
        errno = EIO;
        return -1;
}

/**
 * Redefinition of open(2) system call.
 */
int open( const char *path, int flags, ... ) {
        /* in case openat call was not initialized in the library constructor
           return ELIBACC; open can not return such error in Linux
           and the caller does not expect it and should fail, since we are
           unable to handle this call */
        if ( get_syms()->open == NULL ) {
                errno = ELIBACC;
                return -1;
        }

        /* open() will use mode only in case O_CREAT or O_TMPFILE flags
           specified; there is no need the handle it here; for some cases
           mode variable will simply contain rubbish, but it is ok */
        mode_t mode;
        va_list ap;
        va_start( ap, flags );
        mode = va_arg( ap, mode_t );
        va_end( ap );

        /* make a call at first to get file descriptor first;
           if failed, just return error and do no more actions */
        int fd = get_syms()->open( path, flags, mode );

        if ( fd == -1 ) {
                /* proper errno was set in the open() call */
                return fd;
        }

        /* on errno will be set inside finish_open_common() */
        return finish_open_common( fd, flags );
}

/**
 * Redefinition of openat(2) system call.
 */
int openat( int dir_fd, const char *path, int flags, ... ) {
        /* in case openat call was not initialized in the library constructor
           return ELIBACC; openat can not return such error in Linux
           and the caller does not expect it and should fail, since we are
           unable to handle this call */
        if ( get_syms()->openat == NULL ) {
                errno = ELIBACC;
                return -1;
        }

        /* openat() will use mode only in case O_CREAT or O_TMPFILE flags
           specified; there is no need the handle it here; for some cases
           mode variable will simply contain rubbish, but it is ok */
        mode_t mode;
        va_list ap;
        va_start( ap, flags );
        mode = va_arg( ap, mode_t );
        va_end( ap );

        /* make a call at first to get file descriptor first;
           if failed, just return error and do no more actions */
        int fd = get_syms()->openat( dir_fd, path, flags, mode );

        if ( fd == -1 ) {
                /* proper errno was set in the openat() call */
                return fd;
        }

        /* on errno will be set inside finish_open_common() */
        return finish_open_common( fd, flags );
}
