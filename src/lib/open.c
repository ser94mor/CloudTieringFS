#define _GNU_SOURCE        /* needed for O_TMPFILE */

#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "syms.h"

int open( const char *path, int flags, ... ) {
        int ret = is_file_local( path );

        /* handle the case when the file is local */
        if ( ret && ( ret != -1 ) ) {
                mode_t mode = 0; /* default mode value */

                /* mode parameter is only relevant when O_CREAT or O_TMPFILE
                 *                  specified in flags argument */
                if ( (  ( flags & O_CREAT )   == O_CREAT )
                        || ( flags & O_TMPFILE ) == O_TMPFILE ) {
                        va_list ap;
                va_start(ap, flags);
                mode = va_arg(ap, mode_t);
                va_end(ap);
                        }

                        return get_syms()->open( path, flags, mode );
        }

        /* handle the case when the file is remote */
        if ( ret == 0 ) {

        }
}
