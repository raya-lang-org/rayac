#include "raya_rt.h"
#include <stdio.h>
#include <stdlib.h>

void raya_panic(const char *msg, const char *file, int line) {
    fprintf(stderr, "raya panic at %s:%d: %s\n", file, line, msg);
    abort();
}

void raya_bounds_check(size_t idx, size_t len, const char *file, int line) {
    if (idx >= len) {
        raya_panic("index out of bounds", file, line);
    }
}

void raya_print(raya_Str s) {
    fwrite(s.ptr, 1, s.len, stdout);
}
