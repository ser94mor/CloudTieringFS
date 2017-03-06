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

#include "syms.h"

static symbols_t symbols = { 0 };

/* This function creates a table of pointers to glibc functions for all
 * of the io system calls so they can be called when needed
 */
void __attribute__ ((constructor)) cloudtiering_init_syms(void) {
    symbols.open       = dlsym( RTLD_NEXT, "open"       );
    symbols.open64     = dlsym( RTLD_NEXT, "open64"     );
    symbols.openat     = dlsym( RTLD_NEXT, "openat"     );
    symbols.openat64   = dlsym( RTLD_NEXT, "openat64"   );
    symbols.truncate   = dlsym( RTLD_NEXT, "truncate"   );
    symbols.truncate64 = dlsym( RTLD_NEXT, "truncate64" );
    symbols.stat       = dlsym( RTLD_NEXT, "stat"       );
    symbols.stat64     = dlsym( RTLD_NEXT, "stat64"     );
    symbols.fstatat    = dlsym( RTLD_NEXT, "fstatat"    );
    symbols.fstatat64  = dlsym( RTLD_NEXT, "fstatat64"  );
    symbols.futimesat  = dlsym( RTLD_NEXT, "futimesat"  );
    symbols.utimes     = dlsym( RTLD_NEXT, "utimes"     );
    symbols.utime      = dlsym( RTLD_NEXT, "utime"      );
    symbols.fadvise    = dlsym( RTLD_NEXT, "fadvise"    );
    symbols.fadvise64  = dlsym( RTLD_NEXT, "fadvise64"  );

    symbols.fopen      = dlsym( RTLD_NEXT, "fopen"   );
    symbols.freopen    = dlsym( RTLD_NEXT, "freopen" );
}
