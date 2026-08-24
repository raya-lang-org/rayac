#include "raya_rt.h"
#include <stdio.h>
#include <stdlib.h>

void raya_panic(const char *msg, const char *file, int line) {
    fprintf(stderr, "raya panic at %s:%d: %s\n", file, line, msg);
    abort();
}

void raya_bounds_check(size_t idx, size_t len, const char *file, int line) {
    if (idx >= len) {
        char buf[256];
        snprintf(buf, sizeof(buf), "index out of bounds: %zu >= %zu", idx, len);
        raya_panic(buf, file, line);
    }
}

void raya_print(raya_Str s) {
    fwrite(s.ptr, 1, s.len, stdout);
}
