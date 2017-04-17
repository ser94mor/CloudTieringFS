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

#ifndef CLOUDTIERING_SYMS_H
#define CLOUDTIERING_SYMS_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Since this library aims to be a POSIX-conformant, there is no need to
   implement LFS specification because it is an ptional feature in SUSv2+.
   It is not presented in POSIX */

typedef struct {
    int (*open)( const char *, int, ... );
    int (*openat)( int, const char *, int, ... );

    int (*truncate)( const char *, off_t );

    FILE *(*fopen)( const char *, const char * );
    FILE *(*freopen)( const char *, const char *, FILE * );
} symbols_t;

symbols_t *get_syms( void );

int is_local_file( int fd, int flags );
int clear_xattrs( int fd );
int schedule_download( int fd );
int poll_file_location( int fd, int flags, int should_wait );

#endif /* CLOUDTIERING_SYMS_H */
