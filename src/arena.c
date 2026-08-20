#include "arena.h"

#define ARENA_DEFAULT_CAPACITY (1024 * 1024)

static bool is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static size_t align_up(size_t value, size_t align) {
    assert(is_power_of_two(align));

    size_t mask = align - 1;

    if (value > SIZE_MAX - mask) {
        fprintf(stderr, "arena: size overflow\n");
        abort();
    }

    return (value + mask) & ~mask;
}


void arena_init(Arena* a, size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = ARENA_DEFAULT_CAPACITY;
    a->base = (char*)malloc(initial_capacity);
    if (!a->base) {
        fprintf(stderr, "arena_init: out of memory\n");
        abort();
    }
    a->used = 0;
    a->capacity = initial_capacity;
    a->next = NULL;
}

static Arena* arena_new_chunk(size_t min_size) {
    size_t cap = ARENA_DEFAULT_CAPACITY;
    if (min_size > cap) cap = min_size;
    Arena* chunk = (Arena*)malloc(sizeof(Arena));
    if (!chunk) {
        fprintf(stderr, "arena_new_chunk: out of memory\n");
        abort();
    }
    chunk->base = (char*)malloc(cap);
    if (!chunk->base) {
        free(chunk);
        fprintf(stderr, "arena_new_chunk: out of memory\n");
        abort();
    }
    chunk->used = 0;
    chunk->capacity = cap;
    chunk->next = NULL;
    return chunk;
}

void* arena_alloc(Arena* a, size_t size) {
    return arena_alloc_aligned(a, size, 8);
}

void* arena_alloc_aligned(Arena* a, size_t size, size_t align) {
    if (align == 0 || !is_power_of_two(align)) {
        fprintf(stderr, "arena_alloc_aligned: invalid alignment\n");
        abort();
    }

    Arena* chunk = a;
    while (chunk) {
        size_t aligned = (chunk->used + align - 1) & ~(align - 1);
        if (aligned + size <= chunk->capacity) {
            chunk->used = aligned + size;
            return chunk->base + aligned;
        }
        if (!chunk->next) break;
        chunk = chunk->next;
    }
    
    if (size > SIZE_MAX - align) {
        fprintf(stderr, "arena_alloc_aligned: size overflow\n");
        abort();
    }

    Arena* new_chunk = arena_new_chunk(size + align);
    chunk->next = new_chunk;
    size_t aligned = align_up((size_t)0, align);
    new_chunk->used = aligned + size;
    return new_chunk->base + aligned;
}

void* arena_realloc(Arena* a, void* old_ptr, size_t old_size, size_t new_size) {
    void* new_ptr = arena_alloc(a, new_size);
    if (old_ptr && old_size > 0) {
        memcpy(new_ptr, old_ptr, old_size < new_size ? old_size : new_size);
    }
    return new_ptr;
}

char* arena_strdup(Arena* a, const char* s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char* copy = (char*)arena_alloc(a, len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

char* arena_strndup(Arena* a, const char* s, size_t n) {
    if (!s) {
        return NULL;
    }
    char* copy = (char*)arena_alloc(a, n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

void arena_reset(Arena* a) {
    Arena* chunk = a;
    while (chunk) {
        chunk->used = 0;
        chunk = chunk->next;
    }
}

void arena_free_all(Arena* a) {
    Arena* chunk = a;
    while (chunk) {
        Arena* next = chunk->next;
        free(chunk->base);
        if (chunk != a) free(chunk);
        chunk = next;
    }
    a->base = NULL;
    a->used = 0;
    a->capacity = 0;
    a->next = NULL;
}
