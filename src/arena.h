#ifndef RAYA_ARENA_H
#define RAYA_ARENA_H

#include "common.h"

typedef struct Arena Arena;

struct Arena {
    char* base;
    size_t used;
    size_t capacity;
    Arena* next;
};

void arena_init(Arena* a, size_t initial_capacity);
void* arena_alloc(Arena* a, size_t size);
void* arena_alloc_aligned(Arena* a, size_t size, size_t align);
void* arena_realloc(Arena* a, void* old_ptr, size_t old_size, size_t new_size);
char* arena_strdup(Arena* a, const char* s);
char* arena_strndup(Arena* a, const char* s, size_t n);
void arena_reset(Arena* a);
void arena_free_all(Arena* a);

#define arena_alloc_n(a, T, n) ((T*)arena_alloc_aligned((a), sizeof(T) * (n), alignof(T)))

#endif
