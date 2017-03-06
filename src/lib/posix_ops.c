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

#define _LARGEFILE64_SOURCE    /* needed for off64_t type */
#define _GNU_SOURCE            /* needed for RTLD_NEXT */

#include <dlfcn.h>

#include "posix_ops.h"

static posix_ops_t posix_ops = {
        .open       = NULL,
        .open64     = NULL,
        .openat     = NULL,
        .openat64   = NULL,
        .truncate   = NULL,
        .truncate64 = NULL,
        .stat       = NULL,
        .stat64     = NULL,
        .fstatat    = NULL,
        .fstatat64  = NULL,
        .futimesat  = NULL,
        .utimes     = NULL,
        .utime      = NULL,
        .fadvise    = NULL,
        .fadvise64  = NULL,
};

/* This function creates a table of pointers to glibc functions for all
 * of the io system calls so they can be called when needed
 */
void __attribute__ ((constructor)) cloudtiering_init_posix_ops(void) {
    posix_ops.open       = dlsym( RTLD_NEXT, "open"       );
    posix_ops.open64     = dlsym( RTLD_NEXT, "open64"     );
    posix_ops.openat     = dlsym( RTLD_NEXT, "openat"     );
    posix_ops.openat64   = dlsym( RTLD_NEXT, "openat64"   );
    posix_ops.truncate   = dlsym( RTLD_NEXT, "truncate"   );
    posix_ops.truncate64 = dlsym( RTLD_NEXT, "truncate64" );
    posix_ops.stat       = dlsym( RTLD_NEXT, "stat"       );
    posix_ops.stat64     = dlsym( RTLD_NEXT, "stat64"     );
    posix_ops.fstatat    = dlsym( RTLD_NEXT, "fstatat"    );
    posix_ops.fstatat64  = dlsym( RTLD_NEXT, "fstatat64"  );
    posix_ops.futimesat  = dlsym( RTLD_NEXT, "futimesat"  );
    posix_ops.utimes     = dlsym( RTLD_NEXT, "utimes"     );
    posix_ops.utime      = dlsym( RTLD_NEXT, "utime"      );
    posix_ops.fadvise    = dlsym( RTLD_NEXT, "fadvise"    );
    posix_ops.fadvise64  = dlsym( RTLD_NEXT, "fadvise64"  );
}
