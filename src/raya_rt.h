#ifndef RAYA_RT_H
#define RAYA_RT_H

#include <stdint.h>
#include <stddef.h>

typedef struct { uint8_t const* ptr; size_t len; } raya_Str;
typedef struct { void* ptr; size_t len; } raya_Slice;

void raya_panic(const char *msg, const char *file, int line);
void raya_bounds_check(size_t idx, size_t len, const char *file, int line);
void raya_print(raya_Str s);

#endif
