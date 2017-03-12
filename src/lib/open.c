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
        int ret = is_file_local( path );

        /* handle the case when the file is local */
        if ( ret && ( ret != -1 ) ) {
                mode_t mode = 0; /* default mode value */

                /* mode parameter is only relevant when O_CREAT or O_TMPFILE
                 *                  specified in flags argument */
                if (    ( ( flags & O_CREAT   ) == O_CREAT   )
                     || ( ( flags & O_TMPFILE ) == O_TMPFILE ) ) {
                        va_list ap;
                        va_start( ap, flags );
                        mode = va_arg( ap, mode_t );
                        va_end( ap );
                }

                if ( get_syms()->open == NULL ) {
                        /* use ENOMEM to indicate system error */
                        errno = ENOMEM;
                        return -1;
                }

                return get_syms()->open( path, flags, mode );
        }

        /* handle the case when the file is remote */
        if ( ret == 0 ) {
                if ( initiate_file_download( path ) == -1 ) {
                        /* errno has been set inside that function */
                        return -1;
                }

                poll_file_location( path , 0 );

                if ( get_syms()->open == NULL ) {
                        /* use ENOMEM to indicate system error */
                        errno = ENOMEM;
                        return -1;
                }

                return get_syms()->open( path, flags );
        }

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
