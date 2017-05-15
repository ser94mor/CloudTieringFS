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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "log.h"
#include "file.h"

/* buffer to store error messages (mostly errno messages) */
static __thread char err_buf[ERR_MSG_BUF_LEN];

/* declare array of extended attributes' keys */
static const char *xattr_str[] = {
        XATTRS(XATTR_KEY, COMMA),
};

/**
 * Set an extended attribute to file.
 * See file.h for complete description.
 */
int set_xattr( int fd,
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

/**
 * @brief get_xattr_tunable Get an extended attribute of file.
 *
 * @param[in] fd             File descriptor of file for extended attribute
 *                           to be get.
 * @param[in] xattr          Extended attribute id from the list of known
 *                           extended attributes.
 * @param[in] value          Buffer, large enough to store extended
 *                           attribute's value.
 * @param[in] size           Size of the value buffer.
 * @param[in] ignore_enoattr Flag indicating whether to return error on ENOATTR
 *                           or return special value 1.
 *
 * @return  0: extended attribute has successfully been get and stopred
 *             in value buffer
 *          1: ignore_enoattr has been specified and ENOATTR error was returned
 *             by fgetxattr(2) system call
 *         -1: error has happened while getting an extended attribute of file
 */
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

/**
 * Get an extended attribute of file.
 * See file.h for complete description.
 */
int get_xattr( int fd,
               enum xattr_enum xattr,
               void  *value,
               size_t value_size ) {
        return get_xattr_tunable( fd, xattr, value, value_size, 0 );
}

/**
 * @brief remove_xattr_tunable Remove an extended attribute from file.
 *
 * @param[in] fd             File descriptor of file for extended attribute
 *                           to be removed.
 * @param[in] xattr          Extended attribute id from the list of known
 *                           extended attributes.
 * @param[in] ignore_enoattr Flag indicating whether to return error on ENOATTR
 *                           or return success.
 *
 * @return  0: extended attribute has successfully been removed
 *         -1: error has happened while removing an extended attribute from file
 */
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

/**
 * Remove an extended attribute from file.
 * See file.h for complete description.
 */
int remove_xattr( int fd, enum xattr_enum xattr ) {
        return remove_xattr_tunable( fd, xattr, 0 );
}

/**
 * Try to lock file using extended attribute as an indicator.
 * See file.h for complete description.
 */
int try_lock_file( int fd ) {
        if ( set_xattr( fd, e_locked, NULL, 0, XATTR_CREATE ) == -1 ) {
                /* lock file failures is normal, since another thread or
                   procces may already holding a lock */
                LOG( DEBUG, "failed to lock file [fd: %d]", fd );
                return -1;
        }

        return 0;
}

/**
 * Unlock file via removal of extended attribute.
 * See file.h for complete description.
 */
int unlock_file( int fd ) {
        if ( remove_xattr( fd, e_locked ) == -1 ) {
                /* NOTE: impossible case in case program's logic is correct */

                LOG( ERROR, "failed to unlock file [fd: %d]", fd );
                return -1;
        }

        return 0;
}

/**
 * Check a location of file (local or remote).
 * See file.h for complete description.
 */
int is_local_file_fd( int fd ) {
        /* in case stub attribute is not set returns 1,
           if set returns 0, on error returns -1 */
        return get_xattr_tunable( fd, e_stub, NULL, 0, 1 );
}

/**
 * Check a location of file (local or remote).
 * See file.h for complete description.
 */
int is_local_file_path( const char *path ) {
        /* TODO: implement */
        return -1;
}

/**
 * Check a location of file (local or remote).
 * See file.h for complete description.
 */
int is_remote_file_fd( int fd ) {
        /* in case stub attribute is not set returns 0,
           if set returns 1, on error returns -1 */
        int ret = is_local_file_fd( fd );
        return ( ret == -1 ) ? -1 : ( ! ret );
}

/**
 * Check a location of file (local or remote).
 * See file.h for complete description.
 */
int is_remote_file_path( const char *path ) {
        /* in case stub attribute is not set returns 0,
           if set returns 1, on error returns -1 */
        int ret = is_local_file_path( path );
        return ( ret == -1 ) ? -1 : ( ! ret );
}

/**
 * Checks if file is regular.
 * See file.h for complete description.
 */
int is_regular_file_fd( int fd ) {
        struct stat path_stat;
        if ( ( fstat( fd, &path_stat ) == -1 ) ) {
                return -1;
        }

        return !! ( S_ISREG( path_stat.st_mode ) );
}

/**
 * Checks if file is regular.
 * See file.h for complete description.
 */
int is_regular_file_path( const char *path ) {
        struct stat path_stat;
        if ( ( stat( path, &path_stat ) == -1 ) ) {
                return -1;
        }

        return !! ( S_ISREG( path_stat.st_mode ) );
}
