#pragma once

struct lines {
	uint32_t len, cap;
	char **elems;
};

static struct strview sv_fmtv(const char *fmt, va_list ap);
static struct strview sv_fmt(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
static char *fmt_str(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
static char *strjoin(const char *s1, const char *s2, const char *delim);
static char *subst_file_suffix(const char *file_name, const char *prefix);
static void append_line(struct lines *lines, char *str);
static void log_errorv(const char *filename, struct token *tloc, const char *debug_file, int debug_line,
					   const char *fmt, va_list ap);
static void log_error_impl(const char *filename, struct token *tloc, const char *debug_file, int debug_line,
						   const char *fmt, ...) __attribute__ ((format(printf, 5, 6)));

#define log_error(filename, token, fmt, ...)							\
	log_error_impl(filename, token, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define log_error_and_die(filename, token, fmt, ...)				\
	do {															\
		log_error(filename, token, fmt __VA_OPT__(,) __VA_ARGS__);	\
		EXIT(1);													\
	} while (0)
