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

#include <dlfcn.h>

#include "posix.h"


/* This function creates a table of pointers to glibc functions for all
 * of the io system calls so they can be called when needed
 */
void load_glibc(void)
{
    void *libc_handle;
    libc_handle = dlopen("libc.so.6", RTLD_LAZY|RTLD_GLOBAL);
    if (!libc_handle)
    {
        /* stderr should be set up before this */
        //init_debug("Failed to open libc.so\n");
        libc_handle = RTLD_NEXT;
    }
    memset((void *)&glibc_ops, 0, sizeof(glibc_ops));
    /* first set */
    glibc_ops.snprintf = dlsym(libc_handle, "snprintf");
    glibc_ops.open = dlsym(libc_handle, "open");
    glibc_ops.open64 = dlsym(libc_handle, "open64");
    glibc_ops.openat = dlsym(libc_handle, "openat");
    glibc_ops.openat64 = dlsym(libc_handle, "openat64");
    glibc_ops.creat = dlsym(libc_handle, "creat");
    glibc_ops.creat64 = dlsym(libc_handle, "creat64");
    glibc_ops.unlink = dlsym(libc_handle, "unlink");
    glibc_ops.unlinkat = dlsym(libc_handle, "unlinkat");
    glibc_ops.rename = dlsym(libc_handle, "rename");
    glibc_ops.renameat = dlsym(libc_handle, "renameat");
    glibc_ops.read = dlsym(libc_handle, "read");
    glibc_ops.pread = dlsym(libc_handle, "pread");
    glibc_ops.readv = dlsym(libc_handle, "readv");
    glibc_ops.pread64 = dlsym(libc_handle, "pread64");
    glibc_ops.write = dlsym(libc_handle, "write");
    glibc_ops.pwrite = dlsym(libc_handle, "pwrite");
    glibc_ops.writev = dlsym(libc_handle, "writev");
    glibc_ops.pwrite64 = dlsym(libc_handle, "pwrite64");
    glibc_ops.lseek = dlsym(libc_handle, "lseek");
    glibc_ops.lseek64 = dlsym(libc_handle, "lseek64");
    glibc_ops.perror = dlsym(libc_handle, "perror");
    glibc_ops.truncate = dlsym(libc_handle, "truncate");
    glibc_ops.truncate64 = dlsym(libc_handle, "truncate64");
    glibc_ops.ftruncate = dlsym(libc_handle, "ftruncate");
    glibc_ops.ftruncate64 = dlsym(libc_handle, "ftruncate64");
    glibc_ops.fallocate = dlsym(libc_handle, "posix_fallocate");
    glibc_ops.close = dlsym(libc_handle, "close");
    /* stats */
    glibc_ops.stat = my_glibc_stat;
    glibc_redirect.stat = dlsym(libc_handle, "__xstat");
    glibc_ops.stat64 = my_glibc_stat64;
    glibc_redirect.stat64 = dlsym(libc_handle, "__xstat64");
    glibc_ops.fstat = my_glibc_fstat;
    glibc_redirect.fstat = dlsym(libc_handle, "__fxstat");
    glibc_ops.fstat64 = my_glibc_fstat64;
    glibc_redirect.fstat64 = dlsym(libc_handle, "__fxstat64");
    glibc_ops.fstatat = my_glibc_fstatat;
    glibc_redirect.fstatat = dlsym(libc_handle, "__fxstatat");
    glibc_ops.fstatat64 = my_glibc_fstatat64;
    glibc_redirect.fstatat64 = dlsym(libc_handle, "__fxstatat64");
    glibc_ops.lstat = my_glibc_lstat;
    glibc_redirect.lstat = dlsym(libc_handle, "__lxstat");
    glibc_ops.lstat64 = my_glibc_lstat64;
    glibc_redirect.lstat64 = dlsym(libc_handle, "__lxstat64");
    /* times dups chowns mks */
    glibc_ops.futimesat = dlsym(libc_handle, "futimesat");
    glibc_ops.utimes = dlsym(libc_handle, "utimes");
    glibc_ops.utime = dlsym(libc_handle, "utime");
    glibc_ops.futimes = dlsym(libc_handle, "futimes");
    glibc_ops.dup = dlsym(libc_handle, "dup");
    glibc_ops.dup2 = dlsym(libc_handle, "dup2");
    glibc_ops.dup3 = dlsym(libc_handle, "dup3");
    glibc_ops.chown = dlsym(libc_handle, "chown");
    glibc_ops.fchown = dlsym(libc_handle, "fchown");
    glibc_ops.fchownat = dlsym(libc_handle, "fchownat");
    glibc_ops.lchown = dlsym(libc_handle, "lchown");
    glibc_ops.chmod = dlsym(libc_handle, "chmod");
    glibc_ops.fchmod = dlsym(libc_handle, "fchmod");
    glibc_ops.fchmodat = dlsym(libc_handle, "fchmodat");
    glibc_ops.mkdir = dlsym(libc_handle, "mkdir");
    glibc_ops.mkdirat = dlsym(libc_handle, "mkdirat");
    glibc_ops.rmdir = dlsym(libc_handle, "rmdir");
    glibc_ops.readlink = dlsym(libc_handle, "readlink");
    glibc_ops.readlinkat = dlsym(libc_handle, "readlinkat");
    glibc_ops.symlink = dlsym(libc_handle, "symlink");
    glibc_ops.symlinkat = dlsym(libc_handle, "symlinkat");
    glibc_ops.link = dlsym(libc_handle, "link");
    glibc_ops.linkat = dlsym(libc_handle, "linkat");
    /* readdirs and misc */
    glibc_ops.readdir = my_glibc_readdir;
    glibc_ops.getdents = my_glibc_getdents;
    glibc_ops.getdents64 = my_glibc_getdents64;
    glibc_ops.access = dlsym(libc_handle, "access");
    glibc_ops.faccessat = dlsym(libc_handle, "faccessat");
    glibc_ops.flock = dlsym(libc_handle, "flock");
    glibc_ops.fcntl = dlsym(libc_handle, "fcntl");
    glibc_ops.sync = dlsym(libc_handle, "sync");
    glibc_ops.fsync = dlsym(libc_handle, "fsync");
    glibc_ops.fdatasync = dlsym(libc_handle, "fdatasync");
    glibc_ops.fadvise = my_glibc_fadvise;
    glibc_ops.fadvise64 = my_glibc_fadvise64;
    glibc_ops.statfs = dlsym(libc_handle, "statfs");
    glibc_ops.statfs64 = dlsym(libc_handle, "statfs64");
    glibc_ops.fstatfs = dlsym(libc_handle, "fstatfs");
    glibc_ops.fstatfs64 = dlsym(libc_handle, "fstatfs64");
    glibc_ops.statvfs = dlsym(libc_handle, "statvfs");
    glibc_ops.fstatvfs = dlsym(libc_handle, "fstatvfs");
    glibc_ops.mknod = my_glibc_mknod;
    glibc_redirect.mknod = dlsym(libc_handle, "__xmknod");
    glibc_ops.mknodat = my_glibc_mknodat;
    glibc_redirect.mknodat = dlsym(libc_handle, "__xmknodat");
    glibc_ops.sendfile = dlsym(libc_handle, "sendfile");
    glibc_ops.sendfile64 = dlsym(libc_handle, "sendfile64");
#ifdef HAVE_ATTR_XATTR_H
    glibc_ops.setxattr = dlsym(libc_handle, "setxattr");
    glibc_ops.lsetxattr = dlsym(libc_handle, "lsetxattr");
    glibc_ops.fsetxattr = dlsym(libc_handle, "fsetxattr");
    glibc_ops.getxattr = dlsym(libc_handle, "getxattr");
    glibc_ops.lgetxattr = dlsym(libc_handle, "lgetxattr");
    glibc_ops.fgetxattr = dlsym(libc_handle, "fgetxattr");
    glibc_ops.listxattr = dlsym(libc_handle, "listxattr");
    glibc_ops.llistxattr = dlsym(libc_handle, "llistxattr");
    glibc_ops.flistxattr = dlsym(libc_handle, "flistxattr");
    glibc_ops.removexattr = dlsym(libc_handle, "removexattr");
    glibc_ops.lremovexattr = dlsym(libc_handle, "lremovexattr");
    glibc_ops.fremovexattr = dlsym(libc_handle, "fremovexattr");
#endif
    glibc_ops.socket = dlsym(libc_handle, "socket");
    glibc_ops.accept = dlsym(libc_handle, "accept");
    glibc_ops.bind = dlsym(libc_handle, "bind");
    glibc_ops.connect = dlsym(libc_handle, "connect");
    glibc_ops.getpeername = dlsym(libc_handle, "getpeername");
    glibc_ops.getsockname = dlsym(libc_handle, "getsockname");
    glibc_ops.getsockopt = dlsym(libc_handle, "getsockopt");
    glibc_ops.setsockopt = dlsym(libc_handle, "setsockopt");
    glibc_ops.ioctl = dlsym(libc_handle, "ioctl");
    glibc_ops.listen = dlsym(libc_handle, "listen");
    glibc_ops.recv = dlsym(libc_handle, "recv");
    glibc_ops.recvfrom = dlsym(libc_handle, "recvfrom");
    glibc_ops.recvmsg = dlsym(libc_handle, "recvmsg");
    glibc_ops.send = dlsym(libc_handle, "send");
    glibc_ops.sendto = dlsym(libc_handle, "sendto");
    glibc_ops.sendmsg = dlsym(libc_handle, "sendmsg");
    glibc_ops.shutdown = dlsym(libc_handle, "shutdown");
    glibc_ops.socketpair = dlsym(libc_handle, "socketpair");
    glibc_ops.pipe = dlsym(libc_handle, "pipe");
    glibc_ops.umask = dlsym(libc_handle, "umask");
    glibc_ops.getumask = dlsym(libc_handle, "getumask");
    glibc_ops.getdtablesize = dlsym(libc_handle, "getdtablesize");
    glibc_ops.mmap = dlsym(libc_handle, "mmap");
    glibc_ops.munmap = dlsym(libc_handle, "munmap");
    glibc_ops.msync = dlsym(libc_handle, "msync");
#if 0
    /* might need these some day */
    glibc_ops.acl_delete_def_file = dlsym(libc_handle, "acl_delete_def_file");
    glibc_ops.acl_get_fd = dlsym(libc_handle, "acl_get_fd");
    glibc_ops.acl_get_file = dlsym(libc_handle, "acl_get_file");
    glibc_ops.acl_set_fd = dlsym(libc_handle, "acl_set_fd");
    glibc_ops.acl_set_file = dlsym(libc_handle, "acl_set_file");
#endif
    glibc_ops.getfscreatecon = dlsym(libc_handle, "getfscreatecon");
    glibc_ops.getfilecon = dlsym(libc_handle, "getfilecon");
    glibc_ops.lgetfilecon = dlsym(libc_handle, "lgetfilecon");
    glibc_ops.fgetfilecon = dlsym(libc_handle, "fgetfilecon");
    glibc_ops.setfscreatecon = dlsym(libc_handle, "setfscreatecon");
    glibc_ops.setfilecon = dlsym(libc_handle, "setfilecon");
    glibc_ops.lsetfilecon = dlsym(libc_handle, "lsetfilecon");
    glibc_ops.fsetfilecon = dlsym(libc_handle, "fsetfilecon");

    /* PVFS does not implement socket ops */
    pvfs_ops.socket = dlsym(libc_handle, "socket");
    pvfs_ops.accept = dlsym(libc_handle, "accept");
    pvfs_ops.bind = dlsym(libc_handle, "bind");
    pvfs_ops.connect = dlsym(libc_handle, "connect");
    pvfs_ops.getpeername = dlsym(libc_handle, "getpeername");
    pvfs_ops.getsockname = dlsym(libc_handle, "getsockname");
    pvfs_ops.getsockopt = dlsym(libc_handle, "getsockopt");
    pvfs_ops.setsockopt = dlsym(libc_handle, "setsockopt");
    pvfs_ops.ioctl = dlsym(libc_handle, "ioctl");
    pvfs_ops.listen = dlsym(libc_handle, "listen");
    pvfs_ops.recv = dlsym(libc_handle, "recv");
    pvfs_ops.recvfrom = dlsym(libc_handle, "recvfrom");
    pvfs_ops.recvmsg = dlsym(libc_handle, "recvmsg");
    pvfs_ops.send = dlsym(libc_handle, "send");
    pvfs_ops.sendto = dlsym(libc_handle, "sendto");
    pvfs_ops.sendmsg = dlsym(libc_handle, "sendmsg");
    pvfs_ops.shutdown = dlsym(libc_handle, "shutdown");
    pvfs_ops.socketpair = dlsym(libc_handle, "socketpair");
    pvfs_ops.pipe = dlsym(libc_handle, "pipe");

    /* should have been previously opened */
    /* this decrements the reference count */
    if (libc_handle != RTLD_NEXT)
    {
        dlclose(libc_handle);
    }
}
