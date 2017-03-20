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
#include <sys/types.h>
#include <sys/stat.h>

#include "syms.h"

/* stat() errors:
   EACCES EBADF EFAULT ELOOP ENAMETOOLONG ENOENT ENOMEM ENOTDIR EOVERFLOW */
int open( const char *path, int flags, ... ) {

        /* in case open call was not initialized in the library constructor
           return ENOMEM as it is the only error that indicates real system
           problem */
        if ( get_syms()->open == NULL ) {
                errno = ENOMEM;
                return -1;
        }

        mode_t mode = 0; /* default mode value */

        /* mode parameter is only relevant when O_CREAT or O_TMPFILE
           specified in flags argument */
        if (    ( ( flags & O_CREAT   ) == O_CREAT   )
             || ( ( flags & O_TMPFILE ) == O_TMPFILE ) ) {
                va_list ap;
                va_start( ap, flags );
                mode = va_arg( ap, mode_t );
                va_end( ap );
        }

        /* make a call at first to get file descriptor first;
           if failed, just return error and do no more actions */
        int fd = get_syms()->open( path, flags, mode );

        if ( fd == -1 ) {
                /* proper errno was set in the open() call */
                return fd;
        }

        /* we are here when open() call succeeded (i. e. fd != -1);
           we should determine whether file local or remote and if remote,
           schedule its download in daemon */
        int ret = is_file_local( path );

        /* handle the case when the file is local */
        if ( ret && ( ret != -1 ) ) {
                /* file is local; no need to download it */
                return fd;
        }

        /* handle the case when the file is remote */
        if ( ret == 0 ) {
                if ( initiate_file_download( path ) == -1 ) {
                        /* errno has been set inside that function */
                        return -1;
                }

                if ( poll_file_location( path , 0 ) == -1 ) {
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
                   be handled here */
                /* TODO: handle stat(2) errors */
                return -1;
        }

        /* unreachable place */
        errno = ENOMEM;
        return -1;
}
