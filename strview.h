//#define STRVIEW_IMPLEMENTATION
#ifndef STRVIEW_H_
#define STRVIEW_H_

#ifndef MALLOC
#define MALLOC malloc
#endif
#ifndef REALLOC
#define REALLOC realloc
#endif
#ifndef FREE
#define FREE free
#endif

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct strview {
    size_t len;
    const char  *ptr;
};

bool sv_open_file(const char *filename, struct strview *sv);
bool sv_is_equal(struct strview s1, struct strview s2);
struct strview sv_of_cstr(const char *str);
char *sv_to_cstr(struct strview sv);
struct strview sv_unescape_string(struct strview sv);
bool sv_to_int(struct strview sv, int64_t *out);
bool sv_to_int_base10(struct strview sv, int64_t *out);
bool sv_to_int_base16(struct strview sv, int64_t *out);
struct strview sv_drop_char(struct strview sv);

#define SV_FMT "%.*s"
#define SV_ARGS(sv) ((int)(sv).len), ((sv).ptr)

#endif /* STRVIEW_H_ */

#ifdef STRVIEW_IMPLEMENTATION

bool
sv_open_file(const char *filename, struct strview *sv)
{
    int fd;
    struct stat buf;
    if ((fd = open(filename, O_RDONLY)) < 0)
        return false;
    if (fstat(fd, &buf) < 0) {
        close(fd);
        return false;
    }
    size_t size = buf.st_size;
    char *ptr = MALLOC(size);
    ssize_t len;
    if ((len = read(fd, ptr, size)) < 0) {
        close(fd);
        return false;
    }
    sv->ptr = ptr;
    sv->len = len;
    close(fd);
    return true;
}

bool
sv_is_equal(struct strview s1, struct strview s2)
{
    if (s1.len != s2.len) return false;
    for (size_t i = 0; i < s1.len; ++i) {
        if (s1.ptr[i] != s2.ptr[i]) return false;
    }
    return true;
}

struct strview
sv_of_cstr(const char *str)
{
    return (struct strview){.len = strlen(str), .ptr = str};
}

char *
sv_to_cstr(struct strview sv)
{
    char *s = MALLOC(sv.len + 1);
    assert(s != NULL);
    memcpy(s, sv.ptr, sv.len);
    s[sv.len] = 0;
    return s;
}

static int
char_digit(int c)
{
    switch (c) {
    case '0':           return 0x0;
    case '1':           return 0x1;
    case '2':           return 0x2;
    case '3':           return 0x3;
    case '4':           return 0x4;
    case '5':           return 0x5;
    case '6':           return 0x6;
    case '7':           return 0x7;
    case '8':           return 0x8;
    case '9':           return 0x9;
    case 'a': case 'A': return 0xa;
    case 'b': case 'B': return 0xb;
    case 'c': case 'C': return 0xc;
    case 'd': case 'D': return 0xd;
    case 'e': case 'E': return 0xe;
    case 'f': case 'F': return 0xf;
    default:            return -1;
    }
}

struct strview
sv_unescape_string(struct strview sv)
{
    char *s = MALLOC(sv.len + 1);
    size_t i;
    assert(s != NULL);
    if (sv.len && sv.ptr[0] == '"') sv = sv_drop_char(sv);
    for (i = 0; sv.len; sv = sv_drop_char(sv), ++i) {
        switch (*sv.ptr) {
        case '"': goto done;
        case '\\':
            sv = sv_drop_char(sv);
            assert(sv.len);
            switch (*sv.ptr) {
            case 'a':  s[i] = '\a'; break;
            case 'b':  s[i] = '\b'; break;
            case 'e':  s[i] = '\e'; break;
            case 'f':  s[i] = '\f'; break;
            case 'n':  s[i] = '\n'; break;
            case 'r':  s[i] = '\r'; break;
            case 't':  s[i] = '\t'; break;
            case 'v':  s[i] = '\v'; break;
            case '0':  s[i] = '\0'; break;
            case '\\': s[i] = '\\'; break;
            case '\'': s[i] = '\''; break;
            case '\"': s[i] = '\"'; break;
            case 'x': assert(false && "TODO: escape hex");     break;
            case 'u': assert(false && "TODO: escape unicode"); break;
            case 'U': assert(false && "TODO: escape unicode"); break;
            }
            break;
        default:
            s[i] = *sv.ptr;
        }
    }
done:
    s[i] = 0;
    return (struct strview){.ptr = s, .len = i};
}

bool
sv_to_int_base10(struct strview sv, int64_t *out)
{
    int64_t n = 0;
    for (size_t i = 0; i < sv.len; ++i) {
        if (!isdigit(sv.ptr[i])) return false;
        n = (n * 10) + char_digit(sv.ptr[i]);
    }
    *out = n;
    return true;
}

bool
sv_to_int_base16(struct strview sv, int64_t *out)
{
    int64_t n = 0;
    for (size_t i = 0; i < sv.len; ++i) {
        if (!isxdigit(sv.ptr[i])) return false;
        n = (n * 16) + char_digit(sv.ptr[i]);
    }
    *out = n;
    printf("sv = "SV_FMT"\n", SV_ARGS(sv));
    printf("n  = 0x%lx\n", n);
    return true;
}

bool
sv_to_int(struct strview sv, int64_t *out)
{
    assert(out != NULL);
    if (sv.len == 0) return false;
    if (sv.ptr[0] == '0') {
        if (sv.len == 1) {
            *out = 0;
            return true;
        }
        switch (sv.ptr[1]) {
        case 'x':
        case 'X':
            sv.ptr += 2;
            sv.len -= 2;
            return sv_to_int_base16(sv, out);
        default: assert(false && "TODO: unknown base (sv_to_int)."); break;
        }
    }
    return sv_to_int_base10(sv, out);
}

struct strview
sv_drop_char(struct strview sv)
{
    if (sv.len == 0) return sv;
    sv.ptr++;
    sv.len--;
    return sv;
}

#endif /* STRVIEW_IMPLEMENTATION */
