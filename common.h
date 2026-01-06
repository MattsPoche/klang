#pragma once
#include "da.h"

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof((array)[0]))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define FAILWITH(msg) assert(0 && (msg))
#define UNUSED __attribute__((unused))
