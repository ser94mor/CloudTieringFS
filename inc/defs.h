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

#ifndef CLOUDTIERING_DEFS_H
#define CLOUDTIERING_DEFS_H


/*******************************************************************************
* MACROSES                                                                     *
* --------                                                                     *
*                                                                              *
* A creation of some commoly used definitions is simplified via macroses.      *
* A basic idea is to define a list of names of a similar purpose               *
* (i.e. loggers, protocols, configuration sections etc.) and automate creation *
* of corresponding variables, enums, arrays or anything with simple short      *
* macroses.                                                                    *
*                                                                              *
* A list of entities of some nature is usually defined in a folling way:       *
*         #define ENTITIES(action, sep) \                                      *
*                 action(foo) sep       \                                      *
*                 action(bar) sep       \                                      *
*                 action(goo)                                                  *
* Here 'action' is a macro-function which accepts entity name as an argument   *
* and replaces it with some other thing. 'sep' is a separator which should     *
* separate definitions produced by 'action' macro-function. Set of actions     *
* and separators will be defined in this file.                                 *
*                                                                              *
* Examples:                                                                    *
* (1) Enumerations, arrays:                                                    *
*         #define ENUMERIZE(elem)      e_##elem                                *
*         #define COMMA                ,                                       *
*                                                                              *
*         enum entity_e {                                                      *
*                 ENTITIES(ENUMERIZE, COMMA),                                  *
*         };                                                                   *
*     Preprocessor will transform it to:                                       *
*         enum entity_e {                                                      *
*                 e_foo,                                                       *
*                 e_bar,                                                       *
*                 e_goo,                                                       *
*         };                                                                   *
*                                                                              *
* (2) Number of elements in array, number of defined entities:                 *
*         #define MAP_TO_ONE(elem)     1                                       *
*         #define PLUS                 +                                       *
*                                                                              *
*         size_t entity_count = ENTITIES(MAP_TO_ONE, PLUS);                    *
*     Preprocessor will transform it to:                                       *
*         size_t entity_count = 1 + 1 + 1; // i.e. entity_count = 3            *
*                                                                              *
* (3) Variables' definitions:                                                  *
*         #define DECLARE_ENTITY(elem) const char *str_##elem = #elem          *
*         #define SEMICOLON            ;                                       *
*                                                                              *
*         ENTITIES(DECLARE_ENTITY, SEMICOLON);                                 *
*     Preprocessor will transform it to:                                       *
*         const char *str_foo = "foo";                                         *
*         const char *str_bar = "bar";                                         *
*         const char *str_goo = "goo";                                         *
*******************************************************************************/

/*******************************************************************************
* COMMON CONSTANTS AND MACROSES                                                *
*******************************************************************************/

/* common macroses representing separators */
#define COMMA                ,
#define SEMICOLON            ;
#define PLUS                 +
#define EMPTY

/* common macroses representing macro-functions */
#define ENUMERIZE(elem, ...)         e_##elem
#define STRINGIFY(elem, ...)         #elem
#define MAP_TO_ONE(elem, ...)        1

/* constant string representing program name */
#define PROGRAM_NAME        "cloudtiering"

/* buffer length for error messages; mostly for errno descriptions */
#define ERR_MSG_BUF_LEN     1024

/* the constant representing extended attributes namespace */
#define XATTR_NAMESPACE             "user"

/* template for symlink pointing to the opened file descriptor;
   the lengths of pid_t and int are certainly lesser than
   unsigned loag long int */
#define PROC_PID_FD_FD_PATH_TEMPLATE        "/proc/%llu/fd/%llu"
#define PROC_SELF_FD_FD_PATH_TEMPLATE       "/proc/self/fd/%llu"

/* the maximum possible length of proc_path including '\0' character;
   20 characters to represent pid_t and int is more than enough on any
   possible system */
#define PROC_PID_FD_FD_PATH_MAX_LEN         ( 10 + 20 + 20 + 1 )
#define PROC_SELF_FD_FD_PATH_MAX_LEN        ( 14 + 20 + 1 )

/* a list of all possible extended attributes */
#define XATTRS(action, sep)                                \
        action(stub)        sep \
        action(locked)      sep \
        action(object_id)   sep \
        action(stat)

/* a macro-function producing full name of extended attribute */
#define XATTR_KEY(elem) \
        XATTR_NAMESPACE "." PROGRAM_NAME "." STRINGIFY(elem)

#include <stdint.h>
/* a subset of struct stat fields that will be stored in extended attributes;
   using packed compiler attribute to ensure that we are saving metadata space;
   according to
       http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html
   off_t and blkcnt_t is of signed integer type, and since we may operate in
   a distributed environment with different OSes and file systems configuration,
   we want to use 64-bit signed integer types */
typedef struct {
        /* in stat structure it has off_t type */
        int64_t st_size;

        /* in stat structure it has blkcnt_t type */
        int64_t st_blocks;
} __attribute__ ((__packed__)) stat_t;

#endif    /* CLOUDTIERING_DEFS_H */
