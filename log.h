#pragma once

struct lines {
	uint32_t len, cap;
	char **elems;
};

KC_PUBLIC struct strview sv_fmtv(const char *fmt, va_list ap);
KC_PUBLIC struct strview sv_fmt(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
KC_PUBLIC char *fmt_str(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
KC_PUBLIC char *concat_lines(struct lines *lines, const char *delim);
KC_PUBLIC char *strjoin(const char *s1, const char *s2, const char *delim);
KC_PUBLIC char *subst_file_suffix(const char *file_name, const char *prefix);
KC_PUBLIC void append_line(struct lines *lines, char *str);
KC_PUBLIC void log_errorv(const char *filename, struct token *tloc, const char *debug_file, int debug_line,
					   const char *fmt, va_list ap);
KC_PUBLIC void log_error_impl(const char *filename, struct token *tloc, const char *debug_file, int debug_line,
						   const char *fmt, ...) __attribute__ ((format(printf, 5, 6)));

#define log_error(filename, token, fmt, ...)							\
	log_error_impl(filename, token, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define log_error_and_die(filename, token, fmt, ...)				\
	do {															\
		log_error(filename, token, fmt __VA_OPT__(,) __VA_ARGS__);	\
		EXIT(1);													\
	} while (0)
