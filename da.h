#pragma once

#ifndef DA_MALLOC
#define DA_MALLOC malloc
#endif
#ifndef DA_REALLOC
#define DA_REALLOC realloc
#endif
#ifndef DA_FREE
#define DA_FREE free
#endif

#define DA_INIT_CAP    16
#define da_len(da)     ((da)->len)
#define da_cap(da)     ((da)->cap)
#define da_elems(da)   ((da)->elems)
#define DA_ELEM_SZ(da) (sizeof(*da_elems(da)))

#define da_grow(da)                                                     \
    (da_cap(da) == 0                                                    \
     ? (da_cap(da) = DA_INIT_CAP, da_elems(da) = DA_MALLOC(DA_INIT_CAP * DA_ELEM_SZ(da))) \
     : (da_cap(da) *= 2, da_elems(da) = DA_REALLOC(da_elems(da), da_cap(da) * DA_ELEM_SZ(da))))

#define da_append(da, ...)                                  \
    ((da_len(da) >= da_cap(da) ? da_grow(da) : (void)0),    \
     da_elems(da)[da_len(da)++] = (__VA_ARGS__))

#define da_allot(da)                                                    \
    ((da_append(da, (typeof(0[da_elems(da)])){0})), &da_elems(da)[da_len(da)-1])

#define da_peek(da)                                         \
    (assert(da_len(da) > 0), da_elems(da)[da_len(da) - 1])

#define da_pop(da)                                                      \
    (assert(da_len(da) > 0), da_len(da)--, da_elems(da)[da_len(da)])

#define da_insert(da, idx, ...)                                         \
    do {                                                                \
        if ((idx) >= da_len(da)) {                                      \
            da_append(da, __VA_ARGS__);                                 \
        } else {                                                        \
            size_t _DA_LENGTH = da_len(da);                             \
            da_append(da, (typeof(0[da_elems(da)])){0});                \
            memmove(da_elems(da) + (idx) + 1,                           \
                    da_elems(da) + (idx),                               \
                    (_DA_LENGTH - (idx)) * sizeof(0[da_elems(da)]));    \
            da_elems(da)[idx] = (__VA_ARGS__);                          \
        }                                                               \
    } while (0)

#define da_clear(da) (da_len(da) = 0)

#define da_init(da)                             \
    do {                                        \
        da_len(da) = 0;                         \
        da_cap(da) = 0;                         \
        da_elems(da) = NULL;                    \
    } while (0)

#define da_concat(da_dst, da_src)                           \
    do {                                                    \
        for (size_t __i = 0; __i < da_len(da_src); ++__i) { \
            da_append(da_dst, da_elems(da_src)[__i]);       \
        }                                                   \
    } while (0)

#define da_reverse(da)                              \
    do {                                            \
        size_t __i = 0;                             \
        size_t __j = da_len(da)-1;                  \
        for (; __i < __j; __i++, __j--) {           \
            auto __t = da_elems(da)[__i];           \
            da_elems(da)[__i] = da_elems(da)[__j];  \
            da_elems(da)[__j] = __t;                \
        }                                           \
    } while (0)

#define da_foreach(e, da)                                               \
    for (auto e = &da_elems(da)[0]; (e) < &da_elems(da)[da_len(da)]; ++(e))

#define da_free(da)                                 \
    do {                                            \
        if (da_elems(da)) DA_FREE(da_elems(da));    \
        da_init(da);                                \
    } while (0)
