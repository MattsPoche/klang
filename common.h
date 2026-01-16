#pragma once
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof((array)[0]))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define FAILWITH(_fmt_msg, ...)										\
	do {															\
		fflush(stdout);												\
		fflush(stderr);												\
		fprintf(stderr, "%s: %d: [FAILWITH] ", __FILE__, __LINE__); \
		fprintf(stderr, _fmt_msg __VA_OPT__(,) __VA_ARGS__);		\
		fputc('\n', stderr);										\
		asm("int3");												\
		exit(1);													\
	} while (0)
#define UNUSED __attribute__((unused))
