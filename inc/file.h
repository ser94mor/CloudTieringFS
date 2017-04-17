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


#ifndef CLOUDTIERING_FILE_H
#define CLOUDTIERING_FILE_H

/*******************************************************************************
* USEFUL FILE OPERATIONS                                                       *
* ----------                                                                   *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

#include <sys/types.h>

#include "defs.h"

/* enum of supported extended attributes */
enum xattr_enum {
        XATTRS(ENUMERIZE, COMMA),
};


/*******************************************************************************
* FILE OPERATIONS ON A FILE PATH                                               *
*******************************************************************************/

/**
 * @brief is_local_file_path Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] path Path of file to check location.
 *
 * @return  1: if file is in local storage
 *          0: if file is in remote storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_local_file_path( const char *path );

/**
 * @brief is_remote_file_path Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] path Path of file to check location.
 *
 * @return  1: if file is in remote storage
 *          0: if file is in local storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_remote_file_path( const char *path );

/**
 * @brief is_regular_file_path Checks if file is regular.
 *
 * @param[in] path Path of file to validate.
 *
 * @return  1: if path is valid
 *          0: if path is not valid
 *         -1: error happen during fstat() call
 */
int is_regular_file_path( const char *path );


/*******************************************************************************
* FILE OPERATIONS ON A FILE DESCRIPTOR                                         *
*******************************************************************************/

/**
 * @brief is_local_file_fd Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to check location.
 *
 * @return  1: if file is in local storage
 *          0: if file is in remote storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_local_file_fd( int fd );

/**
 * @brief is_remote_file_fd Check a location of file (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to check location.
 *
 * @return  1: if file is in remote storage
 *          0: if file is in local storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_remote_file_fd( int fd );

/**
 * @brief is_regular_file_fd Checks if file is regular.
 *
 * @param[in] fd File descriptor of file to validate.
 *
 * @return  1: if path is valid
 *          0: if path is not valid
 *         -1: error happen during fstat() call
 */
int is_regular_file_fd( int fd );

/**
 * @brief try_lock_file Try to lock file using extended attribute as an
 *                      indicator.
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to set lock.
 *
 * @return  0: file was locked
 *         -1: file was not locked due to failure or lock was already set on
 *             this file previously
 */
int try_lock_file( int fd );

/**
 * @brief unlock_file Unlock file via removal of extended attribute.
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd File descriptor of file to unset lock.
 *
 * @return  0: file was unlocked
 *         -1: file was not unlocked due to failure
 */
int unlock_file( int fd );

/**
 * @brief set_xattr Set an extended attribute to file.
 *
 * @param[in] fd    File descriptor of file for extended attribute to be set.
 * @param[in] xattr Extended attribute id from the list of known
 *                  extended attributes.
 * @param[in] value Buffer containing extended attribute's value.
 * @param[in] size  Extended attribute's size.
 * @param[in] flags Flags for fsetxattr(2) function.
 *
 * @return  0: extended attribute has successfully been set on file
 *         -1: error has happened while setting extended attribute on file
 */
int set_xattr( int fd,
               enum xattr_enum xattr,
               void *value,
               size_t value_size,
               int flags );

/**
 * @brief get_xattr Get an extended attribute of file.
 *
 * @param[in] fd    File descriptor of file for extended attribute to be get.
 * @param[in] xattr Extended attribute id from the list of known
 *                  extended attributes.
 * @param[in] value Buffer, large enough to store extended attribute's value.
 * @param[in] size  Size of the value buffer.
 *
 * @return  0: extended attribute has successfully been get and stopred
 *             in value buffer
 *         -1: error has happened while getting an extended attribute of file
 */
int get_xattr( int fd,
               enum xattr_enum xattr,
               void  *value,
               size_t value_size );

/**
 * @brief remove_xattr Remove an extended attribute from file.
 *
 * @param[in] fd    File descriptor of file for extended attribute
 *                  to be removed.
 * @param[in] xattr Extended attribute id from the list of known
 *                  extended attributes.
 *
 * @return  0: extended attribute has successfully been removed
 *         -1: error has happened while removing an extended attribute from file
 */
int remove_xattr( int fd, enum xattr_enum xattr );

/*
 * Convenient wrappers around similar functions with path and fd suffixes.
 */
#define is_local_file(arg)                  \
        _Generic((arg),                     \
                 int:     is_local_file_fd, \
                 default: is_local_file_path)(arg)

#define is_remote_file(arg)                  \
        _Generic((arg),                      \
                 int:     is_remote_file_fd, \
                 default: is_remote_file_path)(arg)

#define is_regular_file(arg)                  \
        _Generic((arg),                       \
                 int:     is_regular_file_fd, \
                 default: is_regular_file_path)(arg)

#endif /* CLOUDTIERING_FILE_H */
