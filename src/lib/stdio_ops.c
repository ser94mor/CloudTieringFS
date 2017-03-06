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

#define _GNU_SOURCE            /* needed for RTLD_NEXT */

#include <dlfcn.h>

#include "stdio_ops.h"

static stdio_ops_t stdio_ops = {
        .fopen   = NULL,
        .freopen = NULL,
};

void __attribute__ ((constructor)) cloudtiering_init_stdio_ops( void ) {
        stdio_ops.fopen   = dlsym( RTLD_NEXT, "fopen"   );
        stdio_ops.freopen = dlsym( RTLD_NEXT, "freopen" );
}
