/**
 * @file musl_compat.c
 * @brief musl libc 兼容层 — 为 libstdc++.a (glibc 编译) 提供缺失的符号桩
 *
 * libstdc++.a 在 glibc 环境下编译，引用了 glibc 特有的 __*_chk (fortify) 函数。
 * musl libc 不提供这些函数，此处提供简单转发实现。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>
#include <wchar.h>
#include <sys/types.h>
#include <dirent.h>

// ---- __*_chk 系列：直接转发到对应的标准函数 ----

void* __memcpy_chk(void* dst, const void* src, size_t n, size_t dstlen) {
    (void)dstlen;
    return __builtin_memcpy(dst, src, n);
}

void* __memmove_chk(void* dst, const void* src, size_t n, size_t dstlen) {
    (void)dstlen;
    return __builtin_memmove(dst, src, n);
}

void* __memset_chk(void* dst, int c, size_t n, size_t dstlen) {
    (void)dstlen;
    return __builtin_memset(dst, c, n);
}

char* __strcpy_chk(char* dst, const char* src, size_t dstlen) {
    (void)dstlen;
    return __builtin_strcpy(dst, src);
}

char* __strncpy_chk(char* dst, const char* src, size_t n, size_t dstlen) {
    (void)dstlen;
    return __builtin_strncpy(dst, src, n);
}

char* __strcat_chk(char* dst, const char* src, size_t dstlen) {
    (void)dstlen;
    return __builtin_strcat(dst, src);
}

char* __strncat_chk(char* dst, const char* src, size_t n, size_t dstlen) {
    (void)dstlen;
    return __builtin_strncat(dst, src, n);
}

int __snprintf_chk(char* str, size_t size, int flag, size_t dstlen, const char* format, ...) {
    (void)flag; (void)dstlen;
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    int ret = __builtin_vsnprintf(str, size, format, ap);
    __builtin_va_end(ap);
    return ret;
}

int __sprintf_chk(char* str, int flag, size_t dstlen, const char* format, ...) {
    (void)flag; (void)dstlen;
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    int ret = __builtin_vsprintf(str, format, ap);
    __builtin_va_end(ap);
    return ret;
}

int __vsnprintf_chk(char* str, size_t size, int flag, size_t dstlen, const char* format, __builtin_va_list ap) {
    (void)flag; (void)dstlen;
    return __builtin_vsnprintf(str, size, format, ap);
}

int __vfprintf_chk(FILE* stream, int flag, const char* format, __builtin_va_list ap) {
    (void)flag;
    return vfprintf(stream, format, ap);
}

int __fprintf_chk(FILE* stream, int flag, const char* format, ...) {
    (void)flag;
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    __builtin_va_end(ap);
    return ret;
}

ssize_t __read_chk(int fd, void* buf, size_t nbytes, size_t buflen) {
    (void)buflen;
    return read(fd, buf, nbytes);
}

size_t __fread_chk(void* ptr, size_t size, size_t nmemb, FILE* stream, size_t buflen) {
    (void)buflen;
    return fread(ptr, size, nmemb, stream);
}

int __open_2(const char* pathname, int flags) {
    return open(pathname, flags);
}

int __openat_2(int dirfd, const char* pathname, int flags) {
    return openat(dirfd, pathname, flags);
}

char* __realpath_chk(const char* path, char* resolved_path, size_t resolved_len) {
    (void)resolved_len;
    return realpath(path, resolved_path);
}

ssize_t __readlink_chk(const char* path, char* buf, size_t bufsiz, size_t buflen) {
    (void)buflen;
    return readlink(path, buf, bufsiz);
}

ssize_t __readlinkat_chk(int dirfd, const char* pathname, char* buf, size_t bufsiz, size_t buflen) {
    (void)buflen;
    return readlinkat(dirfd, pathname, buf, bufsiz);
}

void __syslog_chk(int priority, int flag, const char* format, ...) {
    (void)flag;
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    vsyslog(priority, format, ap);
    __builtin_va_end(ap);
}

void __fdelt_chk(long int d) {
    // musl's FD_SET doesn't need checking
    (void)d;
}

size_t __wmemcpy_chk(wchar_t* dst, const wchar_t* src, size_t n, size_t dstlen) {
    (void)dstlen;
    for (size_t i = 0; i < n; i++) dst[i] = src[i];
    return n;
}

size_t __mbsrtowcs_chk(wchar_t* dst, const char** src, size_t len, void* ps, size_t dstlen) {
    (void)dstlen;
    return mbsrtowcs(dst, src, len, (mbstate_t*)ps);
}

// glibc LFS: fseeko64/ftello64 are same as fseeko/ftello on 64-bit
int fseeko64(FILE* stream, off_t offset, int whence) {
    return fseeko(stream, offset, whence);
}
off_t ftello64(FILE* stream) {
    return ftello(stream);
}

// _dl_find_object: glibc internal, libgcc_eh.a needs it
// Provide a dummy stub (unwinding won't work perfectly, but OK for our use case)
int _dl_find_object(void* pc, void* result) {
    (void)pc; (void)result;
    return -1;  // not found
}

// 以下符号在某些 glibc 版本中使用
// musl 不区分 isoc23 版本的 strtol 等

long __isoc23_strtol(const char* nptr, char** endptr, int base) {
    return strtol(nptr, endptr, base);
}

unsigned long __isoc23_strtoul(const char* nptr, char** endptr, int base) {
    return strtoul(nptr, endptr, base);
}

long long __isoc23_strtoll(const char* nptr, char** endptr, int base) {
    return strtoll(nptr, endptr, base);
}

unsigned long long __isoc23_strtoull(const char* nptr, char** endptr, int base) {
    return strtoull(nptr, endptr, base);
}

intmax_t __isoc23_strtoimax(const char* nptr, char** endptr, int base) {
    return strtoimax(nptr, endptr, base);
}

uintmax_t __isoc23_strtoumax(const char* nptr, char** endptr, int base) {
    return strtoumax(nptr, endptr, base);
}

int __isoc23_sscanf(const char* str, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    int ret = vsscanf(str, format, ap);
    __builtin_va_end(ap);
    return ret;
}

// __libc_single_threaded: glibc internal, libstdc++ references it
// musl doesn't have it, define as a proper writable variable
// (using --defsym creates a constant at address 1 which causes SIGSEGV on read)
char __libc_single_threaded = 1;
// Provide simple implementation using /dev/urandom

uint32_t arc4random(void) {
    uint32_t val;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, &val, sizeof(val));
        close(fd);
    } else {
        val = 0;
    }
    return val;
}

void arc4random_buf(void* buf, size_t n) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, buf, n);
        close(fd);
    }
}

// closefrom - musl doesn't have this

void closefrom(int lowfd) {
    DIR* d = opendir("/proc/self/fd");
    if (!d) return;
    int dfd = dirfd(d);
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        int fd = atoi(ent->d_name);
        if (fd >= lowfd && fd != dfd) close(fd);
    }
    closedir(d);
}
