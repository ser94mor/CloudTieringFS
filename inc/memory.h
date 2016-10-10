#ifndef BUFFER_ALLOCATOR_H
#define BUFFER_ALLOCATOR_H

void *allocate_buffer(size_t size);
void *merge_buffers(void *buf1, size_t buf1_size, void *buf2, size_t buf2_size, int free_old_bufs);
void *extend_buffer_right(void *old_buf, size_t old_buf_size, size_t extend_size, int free_old_buf);
void *extend_buffer_left(void *old_buf, size_t old_buf_size, size_t extend_size, int free_old_buf);

#endif // BUFFER_ALLOCATOR_H
