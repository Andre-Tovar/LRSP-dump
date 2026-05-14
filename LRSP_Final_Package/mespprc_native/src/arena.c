#include "internal.h"

#include <stdlib.h>
#include <string.h>

#define MESPPRC_ARENA_DEFAULT_CHUNK (64 * 1024)

static mespprc_arena_chunk_t* arena_alloc_chunk(size_t size) {
    mespprc_arena_chunk_t* chunk = (mespprc_arena_chunk_t*)malloc(sizeof(*chunk));
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

mespprc_arena_t* mespprc_arena_create(size_t default_chunk_size) {
    mespprc_arena_t* arena = (mespprc_arena_t*)malloc(sizeof(*arena));
    if (!arena) return NULL;
    arena->default_chunk_size =
        default_chunk_size ? default_chunk_size : MESPPRC_ARENA_DEFAULT_CHUNK;
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

void* mespprc_arena_alloc(mespprc_arena_t* arena, size_t bytes, size_t align) {
    if (!arena || bytes == 0) return NULL;
    if (align == 0) align = sizeof(void*);

    /* Try to fit in the current head chunk. */
    size_t aligned_used = align_up(arena->head->used, align);
    if (aligned_used + bytes <= arena->head->size) {
        void* ptr = arena->head->base + aligned_used;
        arena->head->used = aligned_used + bytes;
        return ptr;
    }

    /* Allocate a new chunk large enough for the request, doubling growth. */
    size_t next_chunk_size = arena->default_chunk_size;
    while (next_chunk_size < bytes + align) {
        next_chunk_size *= 2;
    }
    mespprc_arena_chunk_t* chunk = arena_alloc_chunk(next_chunk_size);
    if (!chunk) return NULL;
    chunk->next = arena->head;
    arena->head = chunk;

    aligned_used = align_up(chunk->used, align);
    void* ptr = chunk->base + aligned_used;
    chunk->used = aligned_used + bytes;
    return ptr;
}

void* mespprc_arena_calloc(mespprc_arena_t* arena, size_t bytes, size_t align) {
    void* ptr = mespprc_arena_alloc(arena, bytes, align);
    if (ptr) {
        memset(ptr, 0, bytes);
    }
    return ptr;
}

void mespprc_arena_destroy(mespprc_arena_t* arena) {
    if (!arena) return;
    mespprc_arena_chunk_t* chunk = arena->head;
    while (chunk) {
        mespprc_arena_chunk_t* next = chunk->next;
        free(chunk->base);
        free(chunk);
        chunk = next;
    }
    free(arena);
}
