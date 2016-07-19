#include <stdio.h>
#include <string.h>

#include "memory.h"

void *allocate_buffer(size_t size) {
    return malloc(size);
}

void *merge_buffers(void *buf1, size_t buf1_size, void *buf2, size_t buf2_size, int free_old_bufs) {
    void *new_buf = malloc(buf1_size + buf2_size);
    memcpy(new_buf, buf1, buf1_size);
    memcpy(new_buf + buf1_size, buf2, buf2_size);

    if (free_old_bufs) {
        free(buf1);
        free(buf2);
    }

    return new_buf;
}

void *extend_buffer_right(void *old_buf, size_t old_buf_size, size_t extend_size, int free_old_buf) {
    void *new_buf = malloc(old_buf_size + extent_size);
    memcpy(new_buf, old_buf, old_buf_size);
    memset(new_buf + old_buf_size, 0, extend_size);

    if (free_old_buf) {
        free(old_buf);
    }

    return new_buf;
}

void *extend_buffer_left(void *old_buf, size_t old_buf_size, size_t extend_size) {
    void *new_buf = malloc(old_buf_size + extent_size);
    memcpy(new_buf + extend_size, old_buf, old_buf_size);
    memset(new_buf, 0, extend_size);

    if (free_old_buf) {
        free(old_buf);
    }

    return new_buf;
}
