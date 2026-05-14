#ifndef MESPPRC_INTERNAL_H
#define MESPPRC_INTERNAL_H

/*
 * Internal types shared across the C translation units. Nothing in this header
 * is part of the public ABI; the Python binding never sees these structs.
 */

#include <stddef.h>
#include <stdint.h>

#include "mespprc.h"

/* ---------- Arena allocator ----------
 *
 * Bump allocator with chained chunks for solver scratch memory. Per-solve
 * lifetimes are clean and predictable: every label, every bucket, every
 * adjacency array lives in one arena that is freed atomically when the
 * solve handle is destroyed.
 */

typedef struct mespprc_arena_chunk {
    char* base;
    size_t size;
    size_t used;
    struct mespprc_arena_chunk* next;
} mespprc_arena_chunk_t;

typedef struct mespprc_arena {
    mespprc_arena_chunk_t* head;
    size_t default_chunk_size;
} mespprc_arena_t;

mespprc_arena_t* mespprc_arena_create(size_t default_chunk_size);
void* mespprc_arena_alloc(mespprc_arena_t* arena, size_t bytes, size_t align);
void* mespprc_arena_calloc(mespprc_arena_t* arena, size_t bytes, size_t align);
void mespprc_arena_destroy(mespprc_arena_t* arena);

/* ---------- Bitset ----------
 *
 * Customer reachability and visited-set tracking. `num_bits` is logical;
 * storage is rounded up to whole 64-bit words.
 */

typedef struct {
    uint64_t* words;
    int num_bits;
    int num_words;
} mespprc_bitset_t;

void mespprc_bitset_init(mespprc_bitset_t* bs, uint64_t* words, int num_bits);
void mespprc_bitset_clear(mespprc_bitset_t* bs);
int mespprc_bitset_get(const mespprc_bitset_t* bs, int bit);
void mespprc_bitset_set(mespprc_bitset_t* bs, int bit);
void mespprc_bitset_unset(mespprc_bitset_t* bs, int bit);
int mespprc_bitset_count(const mespprc_bitset_t* bs);
int mespprc_bitset_subset_of(const mespprc_bitset_t* a, const mespprc_bitset_t* b);
int mespprc_bitset_equal(const mespprc_bitset_t* a, const mespprc_bitset_t* b);

/* Compute the number of uint64 words required to store `num_bits` bits. */
static inline int mespprc_bitset_words_for(int num_bits) {
    return (num_bits + 63) / 64;
}

/* ---------- Instance internal layout ----------
 *
 * Heap-resident, plain-C-array-of-structs. CSR adjacency lists are populated
 * by mespprc_instance_finalize() so solvers can iterate neighbours without
 * any further allocation.
 */

typedef struct {
    int id;
    int type;
} mespprc_node_internal_t;

typedef struct {
    int tail;
    int head;
    double cost;
    /* `local_res` and `global_res` are stored in flat arrays on the parent
     * instance, indexed by `arc_index * dim + d`. Keeping them out of the
     * header struct keeps the struct cache-friendly and lets us bulk-copy
     * resource vectors when computing label extensions. */
} mespprc_arc_internal_t;

struct mespprc_instance {
    int num_nodes;          /* size of `nodes` */
    int num_arcs;           /* number of arcs added so far */
    int arc_capacity;       /* allocated capacity of `arcs` and resource arrays */
    int local_dim;
    int global_dim;
    int source_id;
    int sink_id;
    int finalized;

    int node_count;         /* number of nodes added so far */

    mespprc_node_internal_t* nodes;        /* num_nodes entries when finalized */
    mespprc_arc_internal_t* arcs;          /* num_arcs entries (length-prefixed by num_arcs) */
    double* arc_local_res;                 /* num_arcs * local_dim doubles */
    double* arc_global_res;                /* num_arcs * global_dim doubles */

    double* local_limits;                  /* local_dim entries */
    double* global_limits;                 /* global_dim entries */

    /* CSR adjacency, populated by finalize. */
    int* out_offset;                       /* num_nodes + 1 entries */
    int* out_arc_index;                    /* num_arcs entries */
    int* in_offset;                        /* num_nodes + 1 entries */
    int* in_arc_index;                     /* num_arcs entries */

    /* Mapping from external node id to dense [0, num_nodes) index, populated
     * incrementally by add_node(). -1 means "unknown id." Sized at create() */
    int max_external_id;
    int* node_id_to_index;                 /* max_external_id + 1 entries */
};

/* Helper: dense node index from external id, or -1 if unknown. */
static inline int mespprc_instance_node_index(const mespprc_instance_t* inst, int external_id) {
    if (external_id < 0 || external_id > inst->max_external_id) {
        return -1;
    }
    return inst->node_id_to_index[external_id];
}

/* ---------- Phase 1 result internal layout ----------
 *
 * Defined here so phase2_*.c can read Phase 1 routes without going through
 * the public byte-by-byte accessor API. All arrays are arena-allocated and
 * owned by `arena`.
 */

struct mespprc_phase1_result {
    int      route_count;
    int      num_customers;
    int      local_dim;
    int      global_dim;

    double*  costs;                 /* route_count */
    int*     path_offsets;          /* route_count + 1 */
    int*     paths;                 /* sum of path lengths, external node ids */
    double*  local_resources;       /* route_count * local_dim */
    double*  global_resources;      /* route_count * global_dim */
    int*     first_customers;       /* route_count, -1 if None */
    int*     customer_state_sigs;   /* route_count * num_customers */

    mespprc_arena_t* arena;
};

#endif /* MESPPRC_INTERNAL_H */
