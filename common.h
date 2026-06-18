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
#include <unistd.h>
#include <execinfo.h>

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof((array)[0]))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define DEBUG_BREAK() __asm__("int3")

#if KC_DEBUG
#    define EXIT(code) do { DEBUG_BREAK(); exit(code); } while (0)
#else
#    define EXIT(code) exit(code)
#endif

#define FAILWITH(_fmt_msg, ...)											\
	do {																\
		fflush(stdout);													\
		fflush(stderr);													\
		fprintf(stderr, "%s: %d: %s: [FAILWITH] ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _fmt_msg __VA_OPT__(,) __VA_ARGS__);			\
		fputc('\n', stderr);											\
		EXIT(1);														\
	} while (0)

#define UNUSED __attribute__((unused))

#define ASSERT(_test, _fmt_msg, ...)									\
	do {																\
		if (!(_test)) {													\
			fprintf(stderr, "%s: %d: %s:\n[ASSERT] assertion failed: (%s)\n", \
					__FILE__, __LINE__, __func__, #_test);				\
			fprintf(stderr, _fmt_msg __VA_OPT__(,) __VA_ARGS__);		\
			fputc('\n', stderr);										\
			EXIT(1);													\
		}																\
	} while (0)

#ifndef KC_PUBLIC
#    define KC_PUBLIC
#endif

#ifndef KC_PUBLIC_DATA
#    define KC_PUBLIC_DATA extern
#endif

#define KC_PRIVATE static

typedef float  float32_t;
typedef double float64_t;

static_assert(sizeof(int32_t) == sizeof(float32_t));
static_assert(sizeof(int64_t) == sizeof(float64_t));

typedef struct kc_session KC_session;

#include "alloc.h"
#include "./da.h"
#include "./strview.h"
#include "./ast.h"
#include "./lex.h"
#include "./parse.h"
#include "./type.h"
#include "./log.h"
#include "./scope.h"
#include "./ir.h"
#include "./linux_system_v_x86_64.h"
#include "./kc.h"
