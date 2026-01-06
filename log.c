#pragma once
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "strview.h"
#include "common.h"
#include "lex.h"
#include "log.h"

struct strview sv_fmtv(const char *fmt, va_list ap)
{
	struct strview sv = {0};
	FILE *stream = open_memstream(&sv.ptr, &sv.len);
	vfprintf(stream, fmt, ap);
	fclose(stream);
	return sv;
}

__attribute__ ((format(printf, 1, 2)))
struct strview sv_fmt(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	struct strview sv = sv_fmtv(fmt, ap);
	va_end(ap);
	return sv;
}

__attribute__ ((format(printf, 1, 2)))
char *fmt_str(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	struct strview sv = sv_fmtv(fmt, ap);
	va_end(ap);
	return sv.ptr;
}

void append_line(struct lines *lines, char *str)
{
	da_append(lines, str);
}

static struct strview current_line(struct strview sv, size_t idx)
{
	char *begin;
	size_t end;
	for (begin = &sv.ptr[idx]; begin > sv.ptr; --begin) {
		if (*begin == '\n') {
			begin++;
			break;
		}
	}
	for (end = idx; end < sv.len && sv.ptr[end] != '\n'; ++end);
	return (struct strview){.ptr = begin, .len = &sv.ptr[end] - begin};
}

void log_errorv(const char *filename, struct strview contents, struct strview svsub,
				struct srcloc loc, const char *debug_file, int debug_line,
				const char *fmt, va_list ap)
{
	fprintf(stderr, "[Error] %s:%d:%d: ", filename, loc.line, loc.column);
	vfprintf(stderr, fmt, ap);
	struct strview line = current_line(contents, svsub.ptr - contents.ptr);
	fprintf(stderr, "\n% 8d | "SV_FMT"\n", loc.line, SV_ARGS(&line));
	fprintf(stderr, "[Debug] %s:%d:\n", debug_file, debug_line);
}

__attribute__ ((format(printf, 7, 8)))
void log_error_impl(const char *filename, struct strview contents, struct strview svsub,
					struct srcloc loc, const char *debug_file, int debug_line,
					const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_errorv(filename, contents, svsub, loc, debug_file, debug_line, fmt, ap);
	va_end(ap);
}
