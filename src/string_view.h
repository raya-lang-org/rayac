
#ifndef RAYA_STRING_VIEW_H
#define RAYA_STRING_VIEW_H

#include "common.h"

typedef struct {
    const char* data;
    size_t len;
} StringView;

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)(sv).len, (sv).data

static inline StringView sv_from_cstr(const char* s) {
    return (StringView){ s, s ? strlen(s) : 0 };
}

static inline StringView sv_from_ptr_len(const char* s, size_t len) {
    return (StringView){ s, len };
}

static inline bool sv_eq(StringView a, StringView b) {
    if (a.len != b.len) return false;
    return memcmp(a.data, b.data, a.len) == 0;
}

static inline bool sv_eq_cstr(StringView a, const char* b) {
    size_t blen = strlen(b);
    if (a.len != blen) return false;
    return memcmp(a.data, b, a.len) == 0;
}

static inline StringView sv_slice(StringView s, size_t start, size_t end) {
    if (start > s.len) start = s.len;
    if (end > s.len) end = s.len;
    if (start > end) { size_t t = start; start = end; end = t; }
    return (StringView){ s.data + start, end - start };
}

#endif
