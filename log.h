#pragma once

struct lines {
	uint32_t len, cap;
	char **elems;
};

struct strview sv_vfmt(const char *fmt, va_list ap);
struct strview sv_fmt(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
char *fmt_str(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
char *strjoin(const char *s1, const char *s2, const char *delim);
void append_line(struct lines *lines, char *str);
void log_errorv(const char *filename, struct strview contents, struct strview svsub,
				struct srcloc loc, const char *debug_file, int debug_line,
				const char *fmt, va_list ap);
void log_error_impl(const char *filename, struct strview contents, struct strview svsub,
					struct srcloc loc, const char *debug_file, int debug_line,
					const char *fmt, ...) __attribute__ ((format(printf, 7, 8)));

#define log_error(filename, contents, svsub, loc, fmt, ...)	\
	log_error_impl(filename, contents, svsub, loc,			\
				   __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define log_error_and_die(filename, contents, svsub, loc, fmt, ...) \
	(log_error(filename, contents, svsub, loc, fmt __VA_OPT__(,) __VA_ARGS__), exit(1))
