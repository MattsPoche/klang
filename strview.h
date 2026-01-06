#ifndef STRVIEW_H_
#define STRVIEW_H_

struct strview {
	size_t len;
	char  *ptr;
};

int64_t file_size(FILE *f);
bool sv_open_file(const char *filename, struct strview *sv);
bool sv_is_equal(struct strview s1, struct strview s2);
struct strview sv_of_cstr(char *str);

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

#endif /* STRVIEW_IMPLEMENTATION */
