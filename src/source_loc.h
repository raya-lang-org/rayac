#ifndef RAYA_SOURCE_LOC_H
#define RAYA_SOURCE_LOC_H

#include "common.h"

typedef struct {
    const char* filename;
    size_t line;
    size_t column;
    size_t offset;
} SourceLocation;

static inline SourceLocation loc_make(const char* filename, size_t line, size_t column, size_t offset) {
    return (SourceLocation){ filename, line, column, offset };
}

#endif
