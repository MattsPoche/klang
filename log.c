#include "common.h"

KC_PUBLIC struct strview
sv_fmtv(const char *fmt, va_list ap)
{
	struct strview sv = {0};
	FILE *stream = open_memstream(&sv.ptr, &sv.len);
	if (fmt == NULL) EXIT(1);
	vfprintf(stream, fmt, ap);
	fclose(stream);
	return sv;
}

KC_PUBLIC struct strview
sv_fmt(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	struct strview sv = sv_fmtv(fmt, ap);
	va_end(ap);
	return sv;
}

KC_PUBLIC char *
fmt_str(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	struct strview sv = sv_fmtv(fmt, ap);
	va_end(ap);
	return sv.ptr;
}

KC_PUBLIC void
append_line(struct lines *lines, char *str)
{
	da_append(lines, str);
}

KC_PUBLIC char *
concat_lines(struct lines *lines, const char *delim)
{
	assert(lines->len > 0);
	size_t delim_len = strlen(delim);
	size_t total_len = (lines->len - 1) * delim_len;
	da_foreach(line, lines) {
		total_len += strlen(*line);
	}
	char *str = malloc(total_len + 1);
	assert(str != NULL);
	for (size_t i = 0; i < total_len; ++i) {
		da_foreach(line, lines) {
			for (size_t j = 0; (*line)[j] != 0; ++j, ++i) {
				str[i] = (*line)[j];
			}
			for (size_t j = 0; delim[j] != 0; ++j, ++i) {
				str[i] = delim[j];
			}
		}
	}
	str[total_len] = 0;
	return str;
}

KC_PUBLIC char *
strjoin(const char *s1, const char *s2, const char *delim)
{
	size_t s1_len = strlen(s1);
	size_t s2_len = strlen(s2);
	size_t delim_len = delim ? strlen(delim) : 0;
	size_t total_len = s1_len + s2_len + delim_len;
	char *cat = malloc(total_len + 1);
	assert(cat != NULL);
	char *cp = cat;
	while (*s1) *cp++ = *s1++;
	if (delim) {
		while (*delim) *cp++ = *delim++;
	}
	while (*s2) *cp++ = *s2++;
	*cp = 0;
	return cat;
}

KC_PUBLIC char *
subst_file_suffix(const char *file_name, const char *prefix)
{
	const char *end_ptr = strrchr(file_name, '.');
	size_t len = end_ptr ? (size_t)(end_ptr - file_name) : strlen(file_name);
	size_t prefix_len = strlen(prefix);
	char *new_name = malloc(len + prefix_len + 2);
	assert(new_name != NULL);
	char *ptr = new_name;
	for (size_t i = 0; i < len; ++i) {
		*ptr++ = file_name[i];
	}
	*ptr++ = '.';
	for (size_t i = 0; i < prefix_len; ++i) {
		*ptr++ = prefix[i];
	}
	*ptr = 0;
	return new_name;
}

KC_PRIVATE struct strview
current_line(struct token *tloc)
{
	size_t begin = tloc->offset;
	size_t end = tloc->offset;
	while (begin > 0 && tloc->text[begin - 1] != '\n') --begin; /* find start of line */
	while (end < tloc->text_len && tloc->text[end] != '\n') ++end; /* find end of line */
	return (struct strview){
		.ptr = tloc->text + begin,
		.len = end - begin,
	};
}

KC_PRIVATE const char *wiggly_line = "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";

KC_PUBLIC void
log_errorv(const char *filename, struct token *tloc, const char *debug_file, int debug_line,
		   const char *fmt, va_list ap)
{
	fprintf(stderr, "[Error] %s:%d:%d: ", filename, tloc->line, tloc->column);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	int left_pad = 8;
	struct strview line = current_line(tloc);
	fprintf(stderr, "%*d | "SV_FMT"\n", left_pad, tloc->line, SV_ARGS(line));
	fprintf(stderr, "%*s | %*s^%.*s\n", left_pad, "", tloc->column, "", (int)tloc->tok_len-1, wiggly_line);
#if KC_DEBUG
	fprintf(stderr, "[Debug] %s:%d:\n", debug_file, debug_line);
#else
	(void)debug_file;
	(void)debug_line;
#endif
}

KC_PUBLIC void
log_error_impl(const char *filename, struct token *tloc,
			   const char *debug_file, int debug_line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_errorv(filename, tloc, debug_file, debug_line, fmt, ap);
	va_end(ap);
}
