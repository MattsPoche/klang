//#define BYTE_BUFFER_IMPLEMENTATION
#ifndef BYTE_BUFFER_H_
#define BYTE_BUFFER_H_

#ifndef NO_STD_HEADERS
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#endif

typedef unsigned char ubyte;
typedef struct byte_buffer Byte_buffer;
typedef void (*byte_buffer_malloc_fn)(Byte_buffer *buf, size_t size);
typedef void (*byte_buffer_free_fn)(Byte_buffer *buf);

struct byte_buffer {
    size_t len, cap;
    size_t cur, ioff;  /* The Cursor. writes/inserts happen at cur+ioff */
    byte_buffer_malloc_fn malloc;
    byte_buffer_malloc_fn realloc;
    byte_buffer_free_fn   free;
    void *data;
};

Byte_buffer *byte_buffer_init(Byte_buffer *buf,
                              size_t init_capacity,
                              byte_buffer_malloc_fn malloc_fn,
                              byte_buffer_malloc_fn realloc_fn,
                              byte_buffer_free_fn   free_fn);
Byte_buffer *byte_buffer_init_default(Byte_buffer *buf);
Byte_buffer byte_buffer_create(size_t init_capacity,
                               byte_buffer_malloc_fn malloc_fn,
                               byte_buffer_malloc_fn realloc_fn,
                               byte_buffer_free_fn   free_fn);
Byte_buffer byte_buffer_create_default(void);
size_t byte_buffer_get_cursor_offset(Byte_buffer *buf);
void byte_buffer_set_cursor_offset(Byte_buffer *buf, size_t offset);
void byte_buffer_set_cursor_start(Byte_buffer *buf);
void byte_buffer_set_cursor_end(Byte_buffer *buf);
void byte_buffer_fill_bytes_at_offset(Byte_buffer *buf, size_t offset, int c, size_t size);
void byte_buffer_write_bytes_at_offset(Byte_buffer *buf, size_t offset, const void *bytes, size_t size);
void byte_buffer_write_bytes(Byte_buffer *buf, const void *bytes, size_t size);
void byte_buffer_fill_bytes(Byte_buffer *buf, int c, size_t size);
void byte_buffer_insert_bytes(Byte_buffer *buf, const void *bytes, size_t size);
void byte_buffer_insert_padding(Byte_buffer *buf, int c, size_t size);
void byte_buffer_put_byte(Byte_buffer *buf, int b);
void byte_buffer_put_str(Byte_buffer *buf, const char *str);
void byte_buffer_free(Byte_buffer *buf);
#ifndef NO_STD_HEADERS
ssize_t byte_buffer_write_data_to_file(Byte_buffer *buf, int fileno);
#endif

#endif /* BYTE_BUFFER_H_ */

#ifdef BYTE_BUFFER_IMPLEMENTATION

#ifndef BB_MALLOC
#define BB_MALLOC malloc
#endif
#ifndef BB_REALLOC
#define BB_REALLOC realloc
#endif
#ifndef BB_FREE
#define BB_FREE free
#endif

#ifndef INIT_BUFFER_SIZE
#define INIT_BUFFER_SIZE PAGE_SIZE
#endif

#ifndef FAILWITH
#define FAILWITH(_fmt_msg, ...)                                     \
    do {                                                            \
        fflush(stdout);                                             \
        fflush(stderr);                                             \
        fprintf(stderr, "%s: %d: [FAILWITH] ", __FILE__, __LINE__); \
        fprintf(stderr, _fmt_msg __VA_OPT__(,) __VA_ARGS__);        \
        fputc('\n', stderr);                                        \
        __asm__("int3");                                            \
        exit(1);                                                    \
    } while (0)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096LU
#endif

static inline size_t
maxzu(size_t x, size_t y)
{
    return x > y ? x : y;
}

static inline void
resize_buffer(Byte_buffer *buf, size_t offset, size_t size)
{
    if (offset + size > buf->cap) {
        size_t new_cap = maxzu(buf->cap * 2, offset + size);
        if (buf->cap == 0) buf->malloc(buf, new_cap);
        else               buf->realloc(buf, new_cap);
    }
    if (offset + size > buf->len) buf->len = offset + size;
}

static void
malloc_mem_buffer(Byte_buffer *buff, size_t size)
{
    if ((buff->data = BB_MALLOC(size)) == NULL) {
#ifndef NO_STD_HEADERS
        FAILWITH("[ERROR] %s\n", strerror(errno));
#else
        FAILWITH(ERROR_MSG("malloc failed"));
#endif
    }
    buff->cap = size;
    buff->len = 0;
}

static void
realloc_mem_buffer(Byte_buffer *buff, size_t size)
{
    if ((buff->data = BB_REALLOC(buff->data, size)) == NULL) {
#ifndef NO_STD_HEADERS
        FAILWITH("[ERROR] %s\n", strerror(errno));
#else
        FAILWITH(ERROR_MSG("realloc failed"));
#endif
    }
    buff->cap = size;
    buff->len = 0;
}

static void
free_mem_buffer(Byte_buffer *buff)
{
    BB_FREE(buff->data);
    buff->cap = 0;
    buff->len = 0;
    buff->data = NULL;
}

Byte_buffer *
byte_buffer_init(Byte_buffer *buf,
                 size_t init_capacity,
                 byte_buffer_malloc_fn malloc_fn,
                 byte_buffer_malloc_fn realloc_fn,
                 byte_buffer_free_fn   free_fn)
{
    buf->malloc  = malloc_fn;
    buf->realloc = realloc_fn;
    buf->free    = free_fn;
    buf->cap     = init_capacity;
    buf->len     = 0;
    buf->cur     = 0;
    buf->ioff    = 0;
    buf->malloc(buf, init_capacity);
    return buf;
}

Byte_buffer *
byte_buffer_init_default(Byte_buffer *buf)
{
    return byte_buffer_init(buf, INIT_BUFFER_SIZE, malloc_mem_buffer, realloc_mem_buffer, free_mem_buffer);
}

Byte_buffer
byte_buffer_create(size_t init_capacity,
                   byte_buffer_malloc_fn malloc_fn,
                   byte_buffer_malloc_fn realloc_fn,
                   byte_buffer_free_fn   free_fn)
{
    Byte_buffer buf;
    byte_buffer_init(&buf, init_capacity, malloc_fn, realloc_fn, free_fn);
    return buf;
}

Byte_buffer
byte_buffer_create_default(void)
{
    Byte_buffer buf;
    byte_buffer_init_default(&buf);
    return buf;
}

size_t
byte_buffer_get_cursor_offset(Byte_buffer *buf)
{
    return buf->cur + buf->ioff;
}

void
byte_buffer_set_cursor_offset(Byte_buffer *buf, size_t offset)
{
    buf->cur = offset;
    buf->ioff = 0;
}

void
byte_buffer_set_cursor_start(Byte_buffer *buf)
{
    byte_buffer_set_cursor_offset(buf, 0);
}

void
byte_buffer_set_cursor_end(Byte_buffer *buf)
{
    byte_buffer_set_cursor_offset(buf, buf->len);
}

void
byte_buffer_fill_bytes_at_offset(Byte_buffer *buf, size_t offset, int c, size_t size)
{
    if (size == 0) return;
    resize_buffer(buf, offset, size);
    memset((ubyte *)buf->data + offset, c, size);
}

void
byte_buffer_write_bytes_at_offset(Byte_buffer *buf, size_t offset, const void *bytes, size_t size)
{
    if (size == 0) return;
    resize_buffer(buf, offset, size);
    memcpy((ubyte *)buf->data + offset, bytes, size);
}

void
byte_buffer_write_bytes(Byte_buffer *buf, const void *bytes, size_t size)
{
    byte_buffer_write_bytes_at_offset(buf, buf->cur + buf->ioff, bytes, size);
    buf->ioff += size;
}

void
byte_buffer_fill_bytes(Byte_buffer *buf, int c, size_t size)
{
    byte_buffer_fill_bytes_at_offset(buf, buf->cur + buf->ioff, c, size);
    buf->ioff += size;
}

void
byte_buffer_insert_bytes(Byte_buffer *buf, const void *bytes, size_t size)
{
    if (size == 0) return;
    size_t offset = buf->cur + buf->ioff;
    if (offset >= buf->len) {
        byte_buffer_write_bytes_at_offset(buf, offset, bytes, size);
    } else {
        size_t move_len = buf->len - offset;
        resize_buffer(buf, buf->len, size);
        memmove((ubyte *)buf->data + offset + size, (ubyte *)buf->data + offset, move_len);
        memcpy((ubyte *)buf->data + offset, bytes, size);
    }
    buf->ioff += size;
}

void
byte_buffer_insert_padding(Byte_buffer *buf, int c, size_t size)
{
    if (size == 0) return;
    size_t offset = buf->cur + buf->ioff;
    if (offset >= buf->len) {
        byte_buffer_fill_bytes_at_offset(buf, offset, c, size);
    } else {
        size_t move_len = buf->len - offset;
        resize_buffer(buf, buf->len, size);
        memmove((ubyte *)buf->data + offset + size, (ubyte *)buf->data + offset, move_len);
        memset((ubyte *)buf->data + offset, c, size);
    }
    buf->ioff += size;
}

void
byte_buffer_put_byte(Byte_buffer *buf, int b)
{
    byte_buffer_insert_bytes(buf, &b, 1);
}

void
byte_buffer_put_str(Byte_buffer *buf, const char *str)
{
    byte_buffer_insert_bytes(buf, str, strlen(str));
}

void
byte_buffer_free(Byte_buffer *buf)
{
    buf->free(buf->data);
    buf->cap = 0;
    buf->len = 0;
    buf->cur = 0;
    buf->ioff = 0;
    buf->data = NULL;
}


#ifndef NO_STD_HEADERS

ssize_t
byte_buffer_write_data_to_file(Byte_buffer *buf, int fileno)
{
    return write(fileno, buf->data, buf->len);
}

#endif

#endif /* BYTE_BUFFER_IMPLEMENTATION */
