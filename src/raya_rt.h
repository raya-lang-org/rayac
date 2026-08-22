#ifndef RAYA_RT_H
#define RAYA_RT_H

#include <stdint.h>
#include <stddef.h>

void raya_panic(const char *msg, const char *file, int line);
void raya_bounds_check(size_t idx, size_t len, const char *file, int line);

#endif
