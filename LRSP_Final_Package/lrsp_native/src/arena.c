/*
 * Bump arena identical in design to mespprc_native/src/arena.c. Vendored as
 * source rather than linked because it is small and lets lrsp_native stay
 * binary-decoupled from the mespprc_native internal API.
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

#define LRSP_ARENA_DEFAULT_CHUNK (64 * 1024)

static lrsp_arena_chunk_t* arena_alloc_chunk(size_t size) {
    lrsp_arena_chunk_t* chunk = (lrsp_arena_chunk_t*)malloc(sizeof(*chunk));
    if (!chunk) return NULL;
    chunk->base = (char*)malloc(size);
    if (!chunk->base) {
        free(chunk);
        return NULL;
    }
    chunk->size = size;
    chunk->used = 0;
    chunk->next = NULL;
    return chunk;
}

lrsp_arena_t* lrsp_arena_create(size_t default_chunk_size) {
    lrsp_arena_t* arena = (lrsp_arena_t*)malloc(sizeof(*arena));
    if (!arena) return NULL;
    arena->default_chunk_size =
        default_chunk_size ? default_chunk_size : LRSP_ARENA_DEFAULT_CHUNK;
    arena->head = arena_alloc_chunk(arena->default_chunk_size);
    if (!arena->head) {
        free(arena);
        return NULL;
    }
    return arena;
}

static size_t align_up(size_t value, size_t alignment) {
    if (alignment <= 1) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

void* lrsp_arena_alloc(lrsp_arena_t* arena, size_t bytes, size_t align) {
    if (!arena || bytes == 0) return NULL;
    if (align == 0) align = sizeof(void*);

    size_t aligned_used = align_up(arena->head->used, align);
    if (aligned_used + bytes <= arena->head->size) {
        void* ptr = arena->head->base + aligned_used;
        arena->head->used = aligned_used + bytes;
        return ptr;
    }

    size_t next_chunk_size = arena->default_chunk_size;
    while (next_chunk_size < bytes + align) {
        next_chunk_size *= 2;
    }
    lrsp_arena_chunk_t* chunk = arena_alloc_chunk(next_chunk_size);
    if (!chunk) return NULL;
    chunk->next = arena->head;
    arena->head = chunk;

    aligned_used = align_up(chunk->used, align);
    void* ptr = chunk->base + aligned_used;
    chunk->used = aligned_used + bytes;
    return ptr;
}

void* lrsp_arena_calloc(lrsp_arena_t* arena, size_t bytes, size_t align) {
    void* ptr = lrsp_arena_alloc(arena, bytes, align);
    if (ptr) memset(ptr, 0, bytes);
    return ptr;
}

void lrsp_arena_destroy(lrsp_arena_t* arena) {
    if (!arena) return;
    lrsp_arena_chunk_t* chunk = arena->head;
    while (chunk) {
        lrsp_arena_chunk_t* next = chunk->next;
        free(chunk->base);
        free(chunk);
        chunk = next;
    }
    free(arena);
}

void lrsp_arena_reset(lrsp_arena_t* arena) {
    /* Reset all chunks. Keep the chain intact; subsequent allocations reuse
     * the existing capacity which is exactly what per-iteration scratch needs. */
    if (!arena) return;
    for (lrsp_arena_chunk_t* c = arena->head; c; c = c->next) {
        c->used = 0;
    }
}

/* ---------- Misc helpers ---------- */

#include <math.h>

uint64_t lrsp_fnv1a_64(const void* data, size_t length, uint64_t seed) {
    const unsigned char* p = (const unsigned char*)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < length; ++i) {
        h ^= (uint64_t)p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

double lrsp_euclidean(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    return sqrt(dx * dx + dy * dy);
}
