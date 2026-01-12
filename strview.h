#ifndef STRVIEW_H_
#define STRVIEW_H_

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"

struct strview {
	size_t len;
	char  *ptr;
};

int64_t file_size(FILE *f);
bool sv_open_file(const char *filename, struct strview *sv);
bool sv_is_equal(struct strview s1, struct strview s2);
struct strview sv_of_cstr(char *str);
bool sv_to_int(struct strview sv, int64_t *out);
bool sv_to_int_base10(struct strview sv, int64_t *out);
bool sv_to_int_base16(struct strview sv, int64_t *out);

#define SV_FMT "%.*s"
#define SV_ARGS(sv_ptr) (int)(sv_ptr)->len, (sv_ptr)->ptr

#endif /* STRVIEW_H_ */

#ifdef STRVIEW_IMPLEMENTATION
#undef STRVIEW_IMPLEMENTATION

int64_t file_size(FILE *f)
{
	long c, s;
	if ((c = ftell(f)) < 0) return -1;
	if (fseek(f, 0, SEEK_END) < 0) return -1;
	s = ftell(f);
	if ((s = ftell(f)) < 0) return -1;
	if (fseek(f, c, SEEK_SET) < 0) return -1;
	return s;
}

bool sv_open_file(const char *filename, struct strview *sv)
{
	FILE *file = fopen(filename, "rb");
	if (file == NULL) return false;
	int64_t len;
	assert((len = file_size(file)) >= 0);
	sv->len = len;
	assert((sv->ptr = malloc(len + 1)));
	int64_t i;
	for (i = 0; i < len; ++i) sv->ptr[i] = fgetc(file);
	sv->ptr[i] = 0;
	fclose(file);
	return true;
}

bool sv_is_equal(struct strview s1, struct strview s2)
{
	if (s1.len != s2.len) return false;
	for (size_t i = 0; i < s1.len; ++i) {
		if (s1.ptr[i] != s2.ptr[i]) return false;
	}
	return true;
}

struct strview sv_of_cstr(char *str)
{
	return (struct strview){.len = strlen(str), .ptr = str};
}

static int conv_tbl[UINT8_MAX] = {
	['0'] = 0x0,
	['1'] = 0x1,
	['2'] = 0x2,
	['3'] = 0x3,
	['4'] = 0x4,
	['5'] = 0x5,
	['6'] = 0x6,
	['7'] = 0x7,
	['8'] = 0x8,
	['9'] = 0x9,
	['a'] = 0xa,
	['b'] = 0xb,
	['c'] = 0xc,
	['d'] = 0xd,
	['e'] = 0xe,
	['f'] = 0xf,
	['A'] = 0xa,
	['B'] = 0xb,
	['C'] = 0xc,
	['D'] = 0xd,
	['E'] = 0xe,
	['F'] = 0xf,
};

bool sv_to_int_base10(struct strview sv, int64_t *out)
{
	int64_t n = 0;
	for (size_t i = 0; i < sv.len; ++i) {
		if (!isdigit(sv.ptr[i])) return false;
		n = (n * 10) + conv_tbl[(size_t)sv.ptr[i]];
	}
	*out = n;
	return true;
}

bool sv_to_int_base16(struct strview sv, int64_t *out)
{
	int64_t n = 0;
	for (size_t i = 0; i < sv.len; ++i) {
		if (!isxdigit(sv.ptr[i])) return false;
		n = (n * 16) + conv_tbl[(size_t)sv.ptr[i]];
	}
	*out = n;
	return true;
}

bool sv_to_int(struct strview sv, int64_t *out)
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
		case 'X': return sv_to_int_base16(sv, out);
		default:  FAILWITH("TODO: unknown base (sv_to_int)."); break;
		}
	}
	return sv_to_int_base10(sv, out);
}

#endif /* STRVIEW_IMPLEMENTATION */
