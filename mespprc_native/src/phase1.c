/*
 * Phase 1 — ESPPRC labeling DP (C port).
 *
 * Mirrors mespprc/phase1.py line for line where it matters. See that file
 * for the canonical specification of:
 *   - the customer-state semantics (REACHABLE / TEMP_UNREACHABLE /
 *     PERM_UNREACHABLE / positive visit-order),
 *   - the dominance partial order with first-customer symmetry breaking, and
 *   - the residual-graph reachability proofs used to mark customers as
 *     permanently unreachable.
 *
 * The port keeps the same identifier names (state_dominates, label_no_worse,
 * refresh_customer_states, ...) so the two implementations can be diffed
 * mechanically when an equivalence failure is investigated.
 */

#include "internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Customer state semantics ---------- *
 * Match label.py constants exactly. */

#define P1_REACHABLE          0
#define P1_TEMP_UNREACHABLE  -2
#define P1_PERM_UNREACHABLE  -1

static inline int p1_is_reachable(int s)        { return s == P1_REACHABLE; }
static inline int p1_is_temp_unreachable(int s) { return s == P1_TEMP_UNREACHABLE; }
static inline int p1_is_perm_unreachable(int s) { return s == P1_PERM_UNREACHABLE; }
static inline int p1_is_visited(int s)          { return s > 0; }
static inline int p1_is_permanently_lost(int s) { return s > 0 || s == P1_PERM_UNREACHABLE; }

/* ---------- Internal types ---------- */

typedef struct {
    int     current_node;
    double  cost;
    double* resources;          /* local_dim */
    int*    unreachable_vector; /* num_customers */
    int     unreachable_count;
} p1_label_t;

typedef struct {
    p1_label_t  label;
    int*        path;            /* path_length */
    int         path_length;
    double*     global_resources;/* global_dim */
    int         first_customer;  /* -1 == None */
} p1_state_t;

typedef struct {
    p1_state_t** items;     /* pointer array, all stored in arena */
    int          count;
    int          capacity;
} p1_bucket_t;

typedef struct {
    const mespprc_instance_t* instance;
    int     num_customers;
    int     num_nodes;
    int     local_dim;
    int     global_dim;
    int     source_node;
    int     sink_node;
    int     label_limit;        /* 0 == no cap */

    /* Mapping: dense node index -> dense customer index, or -1. */
    int*    customer_index_for_node; /* num_nodes entries */
    int*    customer_node_id;        /* num_customers entries */

    /* Lower bounds: for each customer + sink target, for each source node, the
     * minimum-cost local resource vector accumulated along any path. The Python
     * implementation calls these "local_resource_lower_bounds". Layout:
     *
     *   value_at(target, source, dim) =
     *     lower_bounds[((target_idx) * num_nodes + source_idx) * local_dim + dim]
     *
     * targets are: customers (num_customers indices) + sink (one extra index).
     * lower_bounds_target_count = num_customers + 1 (sink last). */
    int     lb_target_count;
    double* lower_bounds;

    /* Per-node label buckets, indexed by dense node index. */
    p1_bucket_t* buckets;

    /* Active node FIFO (matches the Python deque semantics; we always pop the
     * front). Allocated once with capacity `num_nodes` since each node can be
     * active at most once at a time. */
    int* active_queue;
    int  active_count;
    int* in_active_queue;       /* num_nodes; 0/1 flag */

    /* Scratch buffers reused across calls. Each is allocated once at the top
     * of the solve and never freed until the arena dies. */
    int*    scratch_visited;        /* num_nodes  (BFS) */
    int*    scratch_blocked;        /* num_customers */
    int*    scratch_vector;         /* num_customers (refresh working copy) */
    double* scratch_local;          /* local_dim */
    int*    scratch_path;           /* generous upper bound for path length */

    int     scratch_path_capacity;
    int     label_limit_warned;

    mespprc_arena_t* arena;
} p1_ctx_t;

/* mespprc_phase1_result is declared in internal.h so phase2_*.c can read it. */

/* ---------- Forward declarations ---------- */

static p1_state_t* try_extend(p1_ctx_t* ctx, const p1_state_t* state, int succ_dense);
static int insert_with_dominance(p1_ctx_t* ctx, p1_bucket_t* bucket, p1_state_t* candidate);
static int state_dominates(const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b);
static int states_equivalent(const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b);
static int label_no_worse(const p1_ctx_t* ctx, const p1_label_t* a, const p1_label_t* b);
static int label_strictly_better(const p1_ctx_t* ctx, const p1_label_t* a, const p1_label_t* b);
static int customer_state_no_worse(const int* a, const int* b, int n);
static int customer_state_strictly_better(const int* a, const int* b, int n);
static int customer_state_entry_no_worse(int a_value, int b_value);
static int customer_state_entry_strictly_better(int a_value, int b_value);
static int is_better_canonical_phase1_representative(
    const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b);
static int phase1_canonical_key_compare(
    const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b);

static void refresh_customer_states(
    p1_ctx_t* ctx, int current_dense, const double* local_resources, int* vector);
static int proves_permanent_unreachability(
    p1_ctx_t* ctx, int customer_dense, int current_dense,
    const double* local_resources, const int* vector);
static int classify_customer_state(
    p1_ctx_t* ctx, int customer_dense, int current_dense,
    const double* local_resources, const int* vector);

static int can_currently_reach_customer(
    p1_ctx_t* ctx, int customer_dense, int current_dense,
    const double* local_resources, const int* vector);
static int can_still_reach_customer(
    p1_ctx_t* ctx, int customer_dense, int current_dense,
    const double* local_resources, const int* vector);
static int can_reach_sink_after_customer(
    p1_ctx_t* ctx, int customer_dense, const double* after_local, const int* vector);
static int has_residual_path_to_customer(
    p1_ctx_t* ctx, int start_dense, int customer_dense, const int* vector);
static int has_residual_path_after_visiting_customer(
    p1_ctx_t* ctx, int start_dense, int target_dense, int customer_dense,
    const int* vector);
static int can_fit_customer_completion_lower_bound(
    p1_ctx_t* ctx, int current_dense, int customer_dense, const double* local_resources);

static const double* lower_bound_to(p1_ctx_t* ctx, int target_lb_index, int source_dense);
static int compute_local_resource_lower_bounds(p1_ctx_t* ctx);
static int build_initial_state(p1_ctx_t* ctx);

/* Bucket management */
static int bucket_grow(p1_ctx_t* ctx, p1_bucket_t* bucket, int min_capacity);
static int bucket_remove_at(p1_bucket_t* bucket, int index);
static int bucket_append(p1_ctx_t* ctx, p1_bucket_t* bucket, p1_state_t* state);
static void bucket_sort(const p1_ctx_t* ctx, p1_bucket_t* bucket);
static int state_sort_key_compare(
    const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b);

/* Active-queue management */
static void active_push(p1_ctx_t* ctx, int dense);
static int  active_pop(p1_ctx_t* ctx);

/* Vector helpers */
static int vec_le(const double* a, const double* b, int n);
static int vec_lt(const double* a, const double* b, int n);
static int vec_within_limits(const double* v, const double* limit, int n);
static void vec_add(const double* a, const double* b, double* out, int n);
static int vec_compare(const double* a, const double* b, int n);
static int int_vec_compare(const int* a, const int* b, int n);

/* Result building */
static int build_result(
    p1_ctx_t* ctx, mespprc_phase1_result_t* result, mespprc_arena_t* result_arena);

/* ---------- Public entry point ---------- */

mespprc_status_t mespprc_solve_phase1(
    const mespprc_instance_t* instance,
    int label_limit,
    mespprc_phase1_result_t** out_result
) {
    if (!instance || !out_result) return MESPPRC_ERR_INVALID_ARG;
    if (!instance->finalized) return MESPPRC_ERR_NOT_FINALIZED;
    *out_result = NULL;

    /* All Phase 1 scratch lives in one arena that dies with the result. The
     * result keeps a separate arena, so freeing the scratch leaves the result
     * intact. */
    mespprc_arena_t* scratch = mespprc_arena_create(0);
    if (!scratch) return MESPPRC_ERR_NOMEM;

    p1_ctx_t* ctx = mespprc_arena_calloc(scratch, sizeof(p1_ctx_t), 0);
    if (!ctx) {
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    ctx->instance = instance;
    ctx->local_dim = instance->local_dim;
    ctx->global_dim = instance->global_dim;
    ctx->num_nodes = instance->num_nodes;
    ctx->source_node = -1;
    ctx->sink_node = -1;
    ctx->label_limit = label_limit > 0 ? label_limit : 0;
    ctx->arena = scratch;

    /* Build the dense-customer mapping. Customers keep their original ordering
     * by node id (matching Python's `instance.customers()`). */
    ctx->customer_index_for_node =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->num_nodes, sizeof(int));
    if (!ctx->customer_index_for_node) goto err_nomem;
    for (int i = 0; i < ctx->num_nodes; ++i) ctx->customer_index_for_node[i] = -1;

    ctx->customer_node_id =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->num_nodes, sizeof(int));
    if (!ctx->customer_node_id) goto err_nomem;

    int n_cust = 0;
    /* Iterate dense nodes in id order (the instance stores them this way). */
    for (int dense = 0; dense < ctx->num_nodes; ++dense) {
        const mespprc_node_internal_t* node = &instance->nodes[dense];
        if (node->type == MESPPRC_NODE_TYPE_CUSTOMER) {
            ctx->customer_index_for_node[dense] = n_cust;
            ctx->customer_node_id[n_cust] = dense;
            n_cust++;
        }
        if (node->type == MESPPRC_NODE_TYPE_SOURCE) ctx->source_node = dense;
        if (node->type == MESPPRC_NODE_TYPE_SINK)   ctx->sink_node = dense;
    }
    ctx->num_customers = n_cust;
    if (ctx->source_node < 0 || ctx->sink_node < 0) {
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_INSTANCE_INVALID;
    }

    /* Lower bounds: targets are the customers in dense order, then the sink. */
    ctx->lb_target_count = ctx->num_customers + 1;
    if (compute_local_resource_lower_bounds(ctx) != 0) goto err_nomem;

    /* Per-node bucket array. */
    ctx->buckets =
        mespprc_arena_calloc(scratch, sizeof(p1_bucket_t) * (size_t)ctx->num_nodes,
                             sizeof(p1_bucket_t));
    if (!ctx->buckets) goto err_nomem;

    /* Active-node queue. */
    ctx->active_queue =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->num_nodes, sizeof(int));
    ctx->in_active_queue =
        mespprc_arena_calloc(scratch, sizeof(int) * (size_t)ctx->num_nodes, sizeof(int));
    if (!ctx->active_queue || !ctx->in_active_queue) goto err_nomem;
    ctx->active_count = 0;

    /* Scratch buffers. */
    ctx->scratch_visited =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->num_nodes, sizeof(int));
    ctx->scratch_blocked =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->num_customers, sizeof(int));
    ctx->scratch_vector =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->num_customers, sizeof(int));
    if (ctx->local_dim > 0) {
        ctx->scratch_local =
            mespprc_arena_alloc(scratch, sizeof(double) * (size_t)ctx->local_dim,
                                sizeof(double));
    }
    /* Path can never be longer than num_nodes (no node visited twice). */
    ctx->scratch_path_capacity = ctx->num_nodes + 1;
    ctx->scratch_path =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)ctx->scratch_path_capacity,
                            sizeof(int));
    if (!ctx->scratch_visited || !ctx->scratch_blocked || !ctx->scratch_vector
        || (ctx->local_dim > 0 && !ctx->scratch_local)
        || !ctx->scratch_path) {
        goto err_nomem;
    }

    /* Seed the search with the initial state at the source. */
    if (build_initial_state(ctx) != 0) goto err_nomem;

    /* Main labeling loop. Pop one node at a time, extend its bucket. */
    while (ctx->active_count > 0) {
        int node_dense = active_pop(ctx);
        if (node_dense == ctx->sink_node) continue;

        p1_bucket_t* bucket = &ctx->buckets[node_dense];
        if (bucket->count == 0) continue;

        bucket_sort(ctx, bucket);

        /* Snapshot the bucket pointer array because successor inserts may grow
         * other buckets. We iterate by index into a stable snapshot. */
        int snapshot_count = bucket->count;
        p1_state_t** snapshot =
            mespprc_arena_alloc(scratch, sizeof(p1_state_t*) * (size_t)snapshot_count,
                                sizeof(p1_state_t*));
        if (!snapshot) goto err_nomem;
        memcpy(snapshot, bucket->items, sizeof(p1_state_t*) * (size_t)snapshot_count);

        int out_offset = ctx->instance->out_offset[node_dense];
        int out_end    = ctx->instance->out_offset[node_dense + 1];
        for (int s = 0; s < snapshot_count; ++s) {
            const p1_state_t* state = snapshot[s];
            for (int e = out_offset; e < out_end; ++e) {
                int arc_index = ctx->instance->out_arc_index[e];
                int succ_external = ctx->instance->arcs[arc_index].head;
                int succ_dense = mespprc_instance_node_index(ctx->instance, succ_external);
                if (succ_dense < 0) continue;
                p1_state_t* extended = try_extend(ctx, state, succ_dense);
                if (!extended) continue;
                int changed = insert_with_dominance(ctx, &ctx->buckets[succ_dense], extended);
                if (changed) active_push(ctx, succ_dense);
            }
        }
    }

    /* Build the result. */
    mespprc_arena_t* result_arena = mespprc_arena_create(0);
    if (!result_arena) goto err_nomem;
    mespprc_phase1_result_t* result = mespprc_arena_calloc(
        result_arena, sizeof(mespprc_phase1_result_t), 0);
    if (!result) {
        mespprc_arena_destroy(result_arena);
        goto err_nomem;
    }
    result->arena = result_arena;
    result->num_customers = ctx->num_customers;
    result->local_dim     = ctx->local_dim;
    result->global_dim    = ctx->global_dim;

    if (build_result(ctx, result, result_arena) != 0) {
        mespprc_arena_destroy(result_arena);
        goto err_nomem;
    }

    /* Free scratch arena, hand back result. */
    mespprc_arena_destroy(scratch);
    *out_result = result;
    return MESPPRC_OK;

err_nomem:
    mespprc_arena_destroy(scratch);
    return MESPPRC_ERR_NOMEM;
}

void mespprc_phase1_result_destroy(mespprc_phase1_result_t* result) {
    if (!result) return;
    mespprc_arena_t* arena = result->arena;
    /* result lives in arena, so destroying arena frees the struct itself. */
    mespprc_arena_destroy(arena);
}

/* ---------- Result accessors ---------- */

int mespprc_phase1_route_count(const mespprc_phase1_result_t* r) {
    return r ? r->route_count : 0;
}
int mespprc_phase1_num_customers(const mespprc_phase1_result_t* r) {
    return r ? r->num_customers : 0;
}
int mespprc_phase1_local_dim(const mespprc_phase1_result_t* r) {
    return r ? r->local_dim : 0;
}
int mespprc_phase1_global_dim(const mespprc_phase1_result_t* r) {
    return r ? r->global_dim : 0;
}
int mespprc_phase1_path_length(const mespprc_phase1_result_t* r, int idx) {
    if (!r || idx < 0 || idx >= r->route_count) return 0;
    return r->path_offsets[idx + 1] - r->path_offsets[idx];
}

mespprc_status_t mespprc_phase1_route_cost(
    const mespprc_phase1_result_t* r, int idx, double* out_cost) {
    if (!r || !out_cost || idx < 0 || idx >= r->route_count) return MESPPRC_ERR_INVALID_ARG;
    *out_cost = r->costs[idx];
    return MESPPRC_OK;
}
mespprc_status_t mespprc_phase1_route_first_customer(
    const mespprc_phase1_result_t* r, int idx, int* out_first_customer) {
    if (!r || !out_first_customer || idx < 0 || idx >= r->route_count)
        return MESPPRC_ERR_INVALID_ARG;
    *out_first_customer = r->first_customers[idx];
    return MESPPRC_OK;
}
mespprc_status_t mespprc_phase1_route_path(
    const mespprc_phase1_result_t* r, int idx, int* buf, int cap) {
    if (!r || !buf || idx < 0 || idx >= r->route_count) return MESPPRC_ERR_INVALID_ARG;
    int len = r->path_offsets[idx + 1] - r->path_offsets[idx];
    if (cap < len) return MESPPRC_ERR_BUFFER_TOO_SMALL;
    memcpy(buf, r->paths + r->path_offsets[idx], (size_t)len * sizeof(int));
    return MESPPRC_OK;
}
mespprc_status_t mespprc_phase1_route_local_resources(
    const mespprc_phase1_result_t* r, int idx, double* buf, int cap) {
    if (!r || !buf || idx < 0 || idx >= r->route_count) return MESPPRC_ERR_INVALID_ARG;
    if (cap < r->local_dim) return MESPPRC_ERR_BUFFER_TOO_SMALL;
    if (r->local_dim > 0) {
        memcpy(buf, r->local_resources + (size_t)idx * r->local_dim,
               (size_t)r->local_dim * sizeof(double));
    }
    return MESPPRC_OK;
}
mespprc_status_t mespprc_phase1_route_global_resources(
    const mespprc_phase1_result_t* r, int idx, double* buf, int cap) {
    if (!r || !buf || idx < 0 || idx >= r->route_count) return MESPPRC_ERR_INVALID_ARG;
    if (cap < r->global_dim) return MESPPRC_ERR_BUFFER_TOO_SMALL;
    if (r->global_dim > 0) {
        memcpy(buf, r->global_resources + (size_t)idx * r->global_dim,
               (size_t)r->global_dim * sizeof(double));
    }
    return MESPPRC_OK;
}
mespprc_status_t mespprc_phase1_route_customer_state_signature(
    const mespprc_phase1_result_t* r, int idx, int* buf, int cap) {
    if (!r || !buf || idx < 0 || idx >= r->route_count) return MESPPRC_ERR_INVALID_ARG;
    if (cap < r->num_customers) return MESPPRC_ERR_BUFFER_TOO_SMALL;
    if (r->num_customers > 0) {
        memcpy(buf, r->customer_state_sigs + (size_t)idx * r->num_customers,
               (size_t)r->num_customers * sizeof(int));
    }
    return MESPPRC_OK;
}

/* ---------- Initial state ---------- */

static int build_initial_state(p1_ctx_t* ctx) {
    p1_state_t* st = mespprc_arena_calloc(ctx->arena, sizeof(p1_state_t), sizeof(double));
    if (!st) return -1;
    st->label.current_node = ctx->source_node;
    st->label.cost = 0.0;
    if (ctx->local_dim > 0) {
        st->label.resources = mespprc_arena_calloc(
            ctx->arena, sizeof(double) * (size_t)ctx->local_dim, sizeof(double));
        if (!st->label.resources) return -1;
    }
    if (ctx->num_customers > 0) {
        st->label.unreachable_vector = mespprc_arena_calloc(
            ctx->arena, sizeof(int) * (size_t)ctx->num_customers, sizeof(int));
        if (!st->label.unreachable_vector) return -1;
    }
    st->label.unreachable_count = 0;

    if (ctx->global_dim > 0) {
        st->global_resources = mespprc_arena_calloc(
            ctx->arena, sizeof(double) * (size_t)ctx->global_dim, sizeof(double));
        if (!st->global_resources) return -1;
    }
    st->path = mespprc_arena_alloc(ctx->arena, sizeof(int), sizeof(int));
    if (!st->path) return -1;
    st->path[0] = ctx->source_node;
    st->path_length = 1;
    st->first_customer = -1;

    refresh_customer_states(
        ctx, ctx->source_node, st->label.resources, st->label.unreachable_vector);
    /* Recompute unreachable_count after refresh. */
    int cnt = 0;
    for (int i = 0; i < ctx->num_customers; ++i) {
        int v = st->label.unreachable_vector[i];
        if (p1_is_visited(v) || p1_is_perm_unreachable(v)) cnt++;
    }
    st->label.unreachable_count = cnt;

    if (bucket_append(ctx, &ctx->buckets[ctx->source_node], st) != 0) return -1;
    active_push(ctx, ctx->source_node);
    return 0;
}

/* ---------- Active queue ---------- */

static void active_push(p1_ctx_t* ctx, int dense) {
    if (ctx->in_active_queue[dense]) return;
    ctx->active_queue[ctx->active_count++] = dense;
    ctx->in_active_queue[dense] = 1;
}
static int active_pop(p1_ctx_t* ctx) {
    int dense = ctx->active_queue[0];
    /* Shift left by one. The queue is small (bounded by num_nodes) so this
     * naive O(n) pop is fine; Phase 1 wall-clock is dominated by labeling. */
    for (int i = 1; i < ctx->active_count; ++i) {
        ctx->active_queue[i - 1] = ctx->active_queue[i];
    }
    ctx->active_count--;
    ctx->in_active_queue[dense] = 0;
    return dense;
}

/* ---------- Buckets ---------- */

static int bucket_grow(p1_ctx_t* ctx, p1_bucket_t* bucket, int min_capacity) {
    if (bucket->capacity >= min_capacity) return 0;
    int new_cap = bucket->capacity > 0 ? bucket->capacity : 4;
    while (new_cap < min_capacity) new_cap *= 2;
    p1_state_t** items = mespprc_arena_alloc(
        ctx->arena, sizeof(p1_state_t*) * (size_t)new_cap, sizeof(void*));
    if (!items) return -1;
    if (bucket->count > 0) {
        memcpy(items, bucket->items, sizeof(p1_state_t*) * (size_t)bucket->count);
    }
    bucket->items = items;
    bucket->capacity = new_cap;
    return 0;
}
static int bucket_append(p1_ctx_t* ctx, p1_bucket_t* bucket, p1_state_t* state) {
    if (bucket_grow(ctx, bucket, bucket->count + 1) != 0) return -1;
    bucket->items[bucket->count++] = state;
    return 0;
}
static int bucket_remove_at(p1_bucket_t* bucket, int index) {
    if (index < 0 || index >= bucket->count) return -1;
    for (int i = index + 1; i < bucket->count; ++i) {
        bucket->items[i - 1] = bucket->items[i];
    }
    bucket->count--;
    return 0;
}

/* ---------- Insertion with dominance ----------
 *
 * Mirrors Phase1Solver._insert_with_dominance. Returns 1 if the bucket
 * changed (a new state was added or any state was removed/replaced), 0 if
 * the new candidate was equivalent to an existing one or got dominated. */

static int insert_with_dominance(p1_ctx_t* ctx, p1_bucket_t* bucket, p1_state_t* candidate) {
    /* Two-pass: detect equivalence/domination, then drop dominated existing entries. */
    for (int i = 0; i < bucket->count; ++i) {
        const p1_state_t* old = bucket->items[i];
        if (states_equivalent(ctx, old, candidate)) return 0;
        if (state_dominates(ctx, old, candidate))   return 0;
    }
    /* Sweep again, removing existing entries dominated by the new candidate. */
    int i = 0;
    while (i < bucket->count) {
        if (state_dominates(ctx, candidate, bucket->items[i])) {
            bucket_remove_at(bucket, i);
        } else {
            i++;
        }
    }
    if (bucket_append(ctx, bucket, candidate) != 0) return 0;
    bucket_sort(ctx, bucket);

    if (ctx->label_limit > 0 && bucket->count > ctx->label_limit) {
        if (!ctx->label_limit_warned) {
            ctx->label_limit_warned = 1;
            /* Python emits a RuntimeWarning here. The C side stays silent so we
             * don't pull in a logging dependency; the warning flag exists so a
             * later API can expose it as a diagnostic. */
        }
        bucket->count = ctx->label_limit;
    }
    return 1;
}

/* ---------- Equivalence + sort key ---------- */

static int states_equivalent(const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b) {
    if (a->label.current_node != b->label.current_node) return 0;
    if (a->label.cost != b->label.cost) return 0;
    if (vec_compare(a->label.resources, b->label.resources, ctx->local_dim) != 0) return 0;
    if (int_vec_compare(a->label.unreachable_vector, b->label.unreachable_vector,
                        ctx->num_customers) != 0) return 0;
    if (a->label.unreachable_count != b->label.unreachable_count) return 0;
    if (vec_compare(a->global_resources, b->global_resources, ctx->global_dim) != 0) return 0;
    if (a->path_length != b->path_length) return 0;
    if (int_vec_compare(a->path, b->path, a->path_length) != 0) return 0;
    if (a->first_customer != b->first_customer) return 0;
    return 1;
}

static int state_sort_key_compare(
    const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b
) {
    if (a->label.cost < b->label.cost) return -1;
    if (a->label.cost > b->label.cost) return  1;
    int c = vec_compare(a->label.resources, b->label.resources, ctx->local_dim);
    if (c != 0) return c;
    c = vec_compare(a->global_resources, b->global_resources, ctx->global_dim);
    if (c != 0) return c;
    /* first_customer_sort_key: (0, id) for known, (1, +inf) for None. */
    int a_known = (a->first_customer >= 0);
    int b_known = (b->first_customer >= 0);
    if (a_known != b_known) return a_known ? -1 : 1;
    if (a_known) {
        if (a->first_customer < b->first_customer) return -1;
        if (a->first_customer > b->first_customer) return  1;
    }
    int min_len = a->path_length < b->path_length ? a->path_length : b->path_length;
    c = int_vec_compare(a->path, b->path, min_len);
    if (c != 0) return c;
    if (a->path_length < b->path_length) return -1;
    if (a->path_length > b->path_length) return  1;
    return 0;
}

static const p1_ctx_t* g_qsort_ctx = NULL;
static int qsort_state_cmp(const void* lhs, const void* rhs) {
    const p1_state_t* a = *(const p1_state_t* const*)lhs;
    const p1_state_t* b = *(const p1_state_t* const*)rhs;
    return state_sort_key_compare(g_qsort_ctx, a, b);
}
static void bucket_sort(const p1_ctx_t* ctx, p1_bucket_t* bucket) {
    if (bucket->count <= 1) return;
    g_qsort_ctx = ctx;
    qsort(bucket->items, (size_t)bucket->count, sizeof(p1_state_t*), qsort_state_cmp);
    g_qsort_ctx = NULL;
}

/* ---------- Dominance ---------- */

static int state_dominates(const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b) {
    if (a->first_customer >= 0 && b->first_customer >= 0
        && a->first_customer != b->first_customer) {
        return is_better_canonical_phase1_representative(ctx, a, b);
    }
    if (!label_no_worse(ctx, &a->label, &b->label)) return 0;
    if (!vec_le(a->global_resources, b->global_resources, ctx->global_dim)) return 0;
    if (label_strictly_better(ctx, &a->label, &b->label)
        || vec_lt(a->global_resources, b->global_resources, ctx->global_dim)) {
        return 1;
    }
    return is_better_canonical_phase1_representative(ctx, a, b);
}

static int label_no_worse(const p1_ctx_t* ctx, const p1_label_t* a, const p1_label_t* b) {
    if (a->current_node != b->current_node) return 0;
    if (a->cost > b->cost) return 0;
    if (!vec_le(a->resources, b->resources, ctx->local_dim)) return 0;
    if (a->unreachable_count > b->unreachable_count) return 0;
    if (!customer_state_no_worse(a->unreachable_vector, b->unreachable_vector,
                                 ctx->num_customers)) return 0;
    return 1;
}

static int label_strictly_better(const p1_ctx_t* ctx, const p1_label_t* a, const p1_label_t* b) {
    if (a->cost < b->cost) return 1;
    if (vec_lt(a->resources, b->resources, ctx->local_dim)) return 1;
    if (a->unreachable_count < b->unreachable_count) return 1;
    if (customer_state_strictly_better(a->unreachable_vector, b->unreachable_vector,
                                       ctx->num_customers)) return 1;
    return 0;
}

static int customer_state_no_worse(const int* a, const int* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (!customer_state_entry_no_worse(a[i], b[i])) return 0;
    }
    return 1;
}
static int customer_state_strictly_better(const int* a, const int* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (customer_state_entry_strictly_better(a[i], b[i])) return 1;
    }
    return 0;
}

static int customer_state_entry_no_worse(int a_value, int b_value) {
    if (p1_is_reachable(a_value)) return 1;
    if (p1_is_temp_unreachable(a_value)) return !p1_is_reachable(b_value);
    if (p1_is_perm_unreachable(a_value)) return p1_is_perm_unreachable(b_value);
    /* visited */
    return p1_is_visited(b_value);
}
static int customer_state_entry_strictly_better(int a_value, int b_value) {
    if (!customer_state_entry_no_worse(a_value, b_value)) return 0;
    if (p1_is_reachable(a_value)) return !p1_is_reachable(b_value);
    if (p1_is_temp_unreachable(a_value)) {
        return p1_is_perm_unreachable(b_value) || p1_is_visited(b_value);
    }
    return 0;
}

/* Canonical-key tiebreak — used both when first_customers differ in dominance
 * and as a tiebreaker when otherwise-tied labels are compared. Mirrors the
 * Python implementation. */

static int collapsed_customer_states_compare(const int* a, const int* b, int n) {
    /* Python collapses visited to 1; PERM stays -1; TEMP stays -2; REACHABLE 0. */
    for (int i = 0; i < n; ++i) {
        int av = p1_is_visited(a[i]) ? 1 : a[i];
        int bv = p1_is_visited(b[i]) ? 1 : b[i];
        if (av < bv) return -1;
        if (av > bv) return  1;
    }
    return 0;
}

static int phase1_canonical_key_compare(
    const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b
) {
    int a_known = (a->first_customer >= 0);
    int b_known = (b->first_customer >= 0);
    if (a_known != b_known) return a_known ? -1 : 1;
    if (a_known) {
        if (a->first_customer < b->first_customer) return -1;
        if (a->first_customer > b->first_customer) return  1;
    }
    /* Visited-customer tuple (sorted ascending). */
    int n = ctx->num_customers;
    int a_count = 0, b_count = 0;
    for (int i = 0; i < n; ++i) {
        if (p1_is_visited(a->label.unreachable_vector[i])) a_count++;
        if (p1_is_visited(b->label.unreachable_vector[i])) b_count++;
    }
    /* customer_id is the dense customer index here (matches Python because the
     * Python `customer_ids` list iterates in id order). Compare elementwise. */
    int ai = 0, bi = 0;
    int aii = 0, bii = 0;
    int a_seen[1024]; int b_seen[1024];
    /* For larger N, fall back to dynamic comparison (rare). For now, assume
     * num_customers <= 1024 in our benchmark instances. */
    (void)ai; (void)bi; (void)aii; (void)bii;
    int* a_visited = a_seen; int* b_visited = b_seen;
    int ax = 0, bx = 0;
    if (n > 1024) {
        /* Fallback path — allocate transient buffers from the arena. */
        a_visited = mespprc_arena_alloc(ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
        b_visited = mespprc_arena_alloc(ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    }
    for (int i = 0; i < n; ++i) {
        if (p1_is_visited(a->label.unreachable_vector[i])) {
            a_visited[ax++] = ctx->customer_node_id[i];
        }
        if (p1_is_visited(b->label.unreachable_vector[i])) {
            b_visited[bx++] = ctx->customer_node_id[i];
        }
    }
    int min_len = ax < bx ? ax : bx;
    int c = int_vec_compare(a_visited, b_visited, min_len);
    if (c != 0) return c;
    if (ax < bx) return -1;
    if (ax > bx) return  1;
    /* Path tiebreak. */
    int min_path = a->path_length < b->path_length ? a->path_length : b->path_length;
    c = int_vec_compare(a->path, b->path, min_path);
    if (c != 0) return c;
    if (a->path_length < b->path_length) return -1;
    if (a->path_length > b->path_length) return  1;
    return 0;
}

static int is_better_canonical_phase1_representative(
    const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b
) {
    if (a->label.current_node != b->label.current_node) return 0;
    if (a->label.cost != b->label.cost) return 0;
    if (vec_compare(a->label.resources, b->label.resources, ctx->local_dim) != 0) return 0;
    if (vec_compare(a->global_resources, b->global_resources, ctx->global_dim) != 0) return 0;
    if (collapsed_customer_states_compare(
            a->label.unreachable_vector, b->label.unreachable_vector,
            ctx->num_customers) != 0) return 0;
    return phase1_canonical_key_compare(ctx, a, b) < 0;
}

/* ---------- Label extension ---------- */

static p1_state_t* try_extend(p1_ctx_t* ctx, const p1_state_t* state, int succ_dense) {
    int succ_cust_idx = ctx->customer_index_for_node[succ_dense];
    if (succ_cust_idx >= 0
        && !p1_is_reachable(state->label.unreachable_vector[succ_cust_idx])) {
        return NULL;
    }
    int current_dense = state->label.current_node;
    int current_external = ctx->instance->nodes[current_dense].id;
    int succ_external = ctx->instance->nodes[succ_dense].id;
    /* Find the arc; CSR adjacency can be scanned in O(deg). */
    int found_arc = -1;
    int e0 = ctx->instance->out_offset[current_dense];
    int e1 = ctx->instance->out_offset[current_dense + 1];
    for (int e = e0; e < e1; ++e) {
        int arc_index = ctx->instance->out_arc_index[e];
        if (ctx->instance->arcs[arc_index].head == succ_external
            && ctx->instance->arcs[arc_index].tail == current_external) {
            found_arc = arc_index;
            break;
        }
    }
    if (found_arc < 0) return NULL;
    const mespprc_arc_internal_t* arc = &ctx->instance->arcs[found_arc];

    /* Compute new local resources and check limits. */
    if (ctx->local_dim > 0) {
        const double* arc_local = ctx->instance->arc_local_res
            + (size_t)found_arc * ctx->local_dim;
        for (int d = 0; d < ctx->local_dim; ++d) {
            ctx->scratch_local[d] = state->label.resources[d] + arc_local[d];
            if (ctx->scratch_local[d] > ctx->instance->local_limits[d] + 1e-12) return NULL;
        }
    }

    /* Update unreachable vector: succ becomes visited (positive position). */
    memcpy(ctx->scratch_vector, state->label.unreachable_vector,
           sizeof(int) * (size_t)ctx->num_customers);
    int new_first_customer = state->first_customer;
    if (succ_cust_idx >= 0) {
        int next_pos = 1;
        for (int i = 0; i < ctx->num_customers; ++i) {
            if (ctx->scratch_vector[i] >= next_pos) next_pos = ctx->scratch_vector[i] + 1;
        }
        ctx->scratch_vector[succ_cust_idx] = next_pos;
        if (new_first_customer < 0) new_first_customer = succ_external;
    }

    refresh_customer_states(ctx, succ_dense, ctx->scratch_local, ctx->scratch_vector);

    /* Allocate the new state. */
    p1_state_t* st = mespprc_arena_calloc(ctx->arena, sizeof(p1_state_t), sizeof(double));
    if (!st) return NULL;
    st->label.current_node = succ_dense;
    st->label.cost = state->label.cost + arc->cost;

    if (ctx->local_dim > 0) {
        st->label.resources = mespprc_arena_alloc(
            ctx->arena, sizeof(double) * (size_t)ctx->local_dim, sizeof(double));
        if (!st->label.resources) return NULL;
        memcpy(st->label.resources, ctx->scratch_local,
               sizeof(double) * (size_t)ctx->local_dim);
    }
    if (ctx->num_customers > 0) {
        st->label.unreachable_vector = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)ctx->num_customers, sizeof(int));
        if (!st->label.unreachable_vector) return NULL;
        memcpy(st->label.unreachable_vector, ctx->scratch_vector,
               sizeof(int) * (size_t)ctx->num_customers);
    }
    int cnt = 0;
    for (int i = 0; i < ctx->num_customers; ++i) {
        int v = st->label.unreachable_vector[i];
        if (p1_is_visited(v) || p1_is_perm_unreachable(v)) cnt++;
    }
    st->label.unreachable_count = cnt;

    if (ctx->global_dim > 0) {
        st->global_resources = mespprc_arena_alloc(
            ctx->arena, sizeof(double) * (size_t)ctx->global_dim, sizeof(double));
        if (!st->global_resources) return NULL;
        const double* arc_global = ctx->instance->arc_global_res
            + (size_t)found_arc * ctx->global_dim;
        for (int d = 0; d < ctx->global_dim; ++d) {
            st->global_resources[d] = state->global_resources[d] + arc_global[d];
        }
    }

    st->path_length = state->path_length + 1;
    st->path = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)st->path_length, sizeof(int));
    if (!st->path) return NULL;
    memcpy(st->path, state->path, sizeof(int) * (size_t)state->path_length);
    st->path[st->path_length - 1] = succ_dense;
    st->first_customer = new_first_customer;
    return st;
}

/* ---------- Customer state refresh ---------- */

static void refresh_customer_states(
    p1_ctx_t* ctx, int current_dense, const double* local_resources, int* vector
) {
    int n = ctx->num_customers;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < n; ++i) {
            int v = vector[i];
            if (p1_is_visited(v) || p1_is_perm_unreachable(v)) continue;
            if (proves_permanent_unreachability(ctx, i, current_dense, local_resources, vector)) {
                vector[i] = P1_PERM_UNREACHABLE;
                changed = 1;
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        int v = vector[i];
        if (p1_is_visited(v) || p1_is_perm_unreachable(v)) continue;
        vector[i] = classify_customer_state(ctx, i, current_dense, local_resources, vector);
    }
}

static int proves_permanent_unreachability(
    p1_ctx_t* ctx, int cust_idx, int current_dense,
    const double* local_resources, const int* vector
) {
    if (!has_residual_path_to_customer(ctx, current_dense, cust_idx, vector)) return 1;
    if (!has_residual_path_after_visiting_customer(
            ctx, cust_idx /* customer dense index passed via remap below */,
            ctx->sink_node, cust_idx, vector)) return 1;
    if (!can_fit_customer_completion_lower_bound(ctx, current_dense, cust_idx, local_resources))
        return 1;
    return 0;
}

static int classify_customer_state(
    p1_ctx_t* ctx, int cust_idx, int current_dense,
    const double* local_resources, const int* vector
) {
    if (can_currently_reach_customer(ctx, cust_idx, current_dense, local_resources, vector)) {
        return P1_REACHABLE;
    }
    /* Mirror Python: if can_still_reach is true return TEMP; else also TEMP
     * (proves_permanent_unreachability already eliminated permanent cases). */
    (void)can_still_reach_customer(ctx, cust_idx, current_dense, local_resources, vector);
    return P1_TEMP_UNREACHABLE;
}

static int can_currently_reach_customer(
    p1_ctx_t* ctx, int cust_idx, int current_dense,
    const double* local_resources, const int* vector
) {
    int customer_dense = ctx->customer_node_id[cust_idx];
    /* Find arc current -> customer. */
    int e0 = ctx->instance->out_offset[current_dense];
    int e1 = ctx->instance->out_offset[current_dense + 1];
    int found_arc = -1;
    int customer_external = ctx->instance->nodes[customer_dense].id;
    for (int e = e0; e < e1; ++e) {
        int idx = ctx->instance->out_arc_index[e];
        if (ctx->instance->arcs[idx].head == customer_external) {
            found_arc = idx; break;
        }
    }
    if (found_arc < 0) return 0;
    /* Local resource feasibility after stepping. */
    if (ctx->local_dim > 0) {
        const double* arc_local = ctx->instance->arc_local_res
            + (size_t)found_arc * ctx->local_dim;
        double after[16]; /* generous upper bound; we never have >16 dims */
        double* buf = after;
        if (ctx->local_dim > 16) {
            buf = mespprc_arena_alloc(ctx->arena, sizeof(double) * (size_t)ctx->local_dim,
                                      sizeof(double));
        }
        for (int d = 0; d < ctx->local_dim; ++d) {
            buf[d] = local_resources[d] + arc_local[d];
            if (buf[d] > ctx->instance->local_limits[d] + 1e-12) return 0;
        }
        return can_reach_sink_after_customer(ctx, cust_idx, buf, vector);
    }
    return can_reach_sink_after_customer(ctx, cust_idx, NULL, vector);
}

static int can_still_reach_customer(
    p1_ctx_t* ctx, int cust_idx, int current_dense,
    const double* local_resources, const int* vector
) {
    if (!has_residual_path_to_customer(ctx, current_dense, cust_idx, vector)) return 0;
    if (!has_residual_path_after_visiting_customer(
            ctx, cust_idx, ctx->sink_node, cust_idx, vector)) return 0;
    return can_fit_customer_completion_lower_bound(ctx, current_dense, cust_idx, local_resources);
}

static int can_reach_sink_after_customer(
    p1_ctx_t* ctx, int cust_idx, const double* after_local, const int* vector
) {
    /* "after visiting customer, can we still reach the sink?" The Python code
     * tests:
     *   1) residual path from customer to sink under the current vector, and
     *   2) lower-bound completion for customer-to-sink fits in the remaining
     *      local resource budget.
     */
    if (!has_residual_path_after_visiting_customer(
            ctx, cust_idx, ctx->sink_node, cust_idx, vector)) return 0;
    if (ctx->local_dim == 0) return 1;
    /* lb_target for sink is the last entry: lb_target_count - 1. */
    const double* lb = lower_bound_to(ctx, ctx->lb_target_count - 1,
                                      ctx->customer_node_id[cust_idx]);
    for (int d = 0; d < ctx->local_dim; ++d) {
        if (!isfinite(lb[d])) return 0;
        if (after_local[d] + lb[d] > ctx->instance->local_limits[d] + 1e-12) return 0;
    }
    return 1;
}

/* ---------- Residual graph BFS ---------- */

static void compute_blocked_customers(
    const p1_ctx_t* ctx, const int* vector, int allowed_a, int allowed_b, int allowed_c,
    int* blocked_dense
) {
    /* `blocked_dense` is an array of dense node ids (length num_customers).
     * We mark customer nodes that are permanently lost AND not in the
     * allowed set as blocked. */
    for (int i = 0; i < ctx->num_customers; ++i) blocked_dense[i] = 0;
    for (int i = 0; i < ctx->num_customers; ++i) {
        int dense = ctx->customer_node_id[i];
        if (dense == allowed_a || dense == allowed_b || dense == allowed_c) continue;
        if (p1_is_permanently_lost(vector[i])) blocked_dense[i] = 1;
    }
}

static int has_path_in_residual_graph(
    p1_ctx_t* ctx, int start_dense, int target_dense, const int* blocked_customers_dense
) {
    if (start_dense == target_dense) return 1;
    /* BFS using ctx->scratch_visited as the "seen" array. */
    int* seen = ctx->scratch_visited;
    for (int i = 0; i < ctx->num_nodes; ++i) seen[i] = 0;
    seen[start_dense] = 1;
    /* Use scratch_path (capacity == num_nodes + 1) as the BFS queue. */
    int* queue = ctx->scratch_path;
    int qhead = 0, qtail = 0;
    queue[qtail++] = start_dense;
    while (qhead < qtail) {
        int u = queue[qhead++];
        int e0 = ctx->instance->out_offset[u];
        int e1 = ctx->instance->out_offset[u + 1];
        for (int e = e0; e < e1; ++e) {
            int arc_index = ctx->instance->out_arc_index[e];
            int v_external = ctx->instance->arcs[arc_index].head;
            int v_dense = mespprc_instance_node_index(ctx->instance, v_external);
            if (v_dense < 0 || seen[v_dense]) continue;
            /* Block customer nodes flagged in blocked_customers_dense. start
             * and target are exempt (handled by caller via allowed_* params). */
            if (v_dense != target_dense && v_dense != start_dense) {
                int v_cust = ctx->customer_index_for_node[v_dense];
                if (v_cust >= 0 && blocked_customers_dense[v_cust]) continue;
            }
            if (v_dense == target_dense) return 1;
            seen[v_dense] = 1;
            queue[qtail++] = v_dense;
        }
    }
    return 0;
}

static int has_residual_path_to_customer(
    p1_ctx_t* ctx, int start_dense, int customer_idx, const int* vector
) {
    int customer_dense = ctx->customer_node_id[customer_idx];
    int* blocked = ctx->scratch_blocked;
    compute_blocked_customers(ctx, vector, customer_dense, start_dense, -1, blocked);
    return has_path_in_residual_graph(ctx, start_dense, customer_dense, blocked);
}

static int has_residual_path_after_visiting_customer(
    p1_ctx_t* ctx, int customer_idx, int target_dense, int customer_idx_for_block,
    const int* vector
) {
    /* The Python signature is (start, target, customer_id, vector); start is
     * the customer node, target is the sink, customer_id is exempt from
     * blocking. We pass start as the customer's dense node. */
    int customer_dense = ctx->customer_node_id[customer_idx];
    int* blocked = ctx->scratch_blocked;
    int customer_dense_block = ctx->customer_node_id[customer_idx_for_block];
    (void)customer_dense_block; /* same as customer_dense in practice */
    compute_blocked_customers(ctx, vector, customer_dense, customer_dense, target_dense, blocked);
    return has_path_in_residual_graph(ctx, customer_dense, target_dense, blocked);
}

static int can_fit_customer_completion_lower_bound(
    p1_ctx_t* ctx, int current_dense, int customer_idx, const double* local_resources
) {
    if (ctx->local_dim == 0) return 1;
    int customer_dense = ctx->customer_node_id[customer_idx];
    const double* lb_to_cust = lower_bound_to(ctx, customer_idx, current_dense);
    const double* lb_cust_to_sink = lower_bound_to(
        ctx, ctx->lb_target_count - 1, customer_dense);
    for (int d = 0; d < ctx->local_dim; ++d) {
        if (!isfinite(lb_to_cust[d])) return 0;
        if (!isfinite(lb_cust_to_sink[d])) return 0;
        double total = local_resources[d] + lb_to_cust[d] + lb_cust_to_sink[d];
        if (total > ctx->instance->local_limits[d] + 1e-12) return 0;
    }
    return 1;
}

static const double* lower_bound_to(p1_ctx_t* ctx, int target_lb_index, int source_dense) {
    /* Layout: ((target * num_nodes) + source) * local_dim */
    return ctx->lower_bounds
        + (((size_t)target_lb_index * ctx->num_nodes) + source_dense) * ctx->local_dim;
}

static int compute_local_resource_lower_bounds(p1_ctx_t* ctx) {
    if (ctx->local_dim == 0) return 0;
    size_t total = (size_t)ctx->lb_target_count * (size_t)ctx->num_nodes
                 * (size_t)ctx->local_dim;
    ctx->lower_bounds = mespprc_arena_alloc(
        ctx->arena, sizeof(double) * total, sizeof(double));
    if (!ctx->lower_bounds) return -1;
    for (size_t i = 0; i < total; ++i) ctx->lower_bounds[i] = INFINITY;

    /* Reverse Bellman-Ford: distance[v][target] for each local-resource
     * dimension. The Python code does this dimension-by-dimension. */
    for (int t = 0; t < ctx->lb_target_count; ++t) {
        int target_dense = (t == ctx->lb_target_count - 1)
            ? ctx->sink_node
            : ctx->customer_node_id[t];
        for (int d = 0; d < ctx->local_dim; ++d) {
            /* Initialize distances. */
            double* base = (double*)lower_bound_to(ctx, t, 0); /* row begin */
            (void)base;
            for (int v = 0; v < ctx->num_nodes; ++v) {
                ctx->lower_bounds[
                    (((size_t)t * ctx->num_nodes) + v) * ctx->local_dim + d
                ] = INFINITY;
            }
            ctx->lower_bounds[
                (((size_t)t * ctx->num_nodes) + target_dense) * ctx->local_dim + d
            ] = 0.0;
            int updated = 1;
            int iter = 0;
            while (updated && iter < ctx->num_nodes) {
                updated = 0;
                for (int a = 0; a < ctx->instance->num_arcs; ++a) {
                    const mespprc_arc_internal_t* arc = &ctx->instance->arcs[a];
                    int tail_dense = mespprc_instance_node_index(ctx->instance, arc->tail);
                    int head_dense = mespprc_instance_node_index(ctx->instance, arc->head);
                    if (tail_dense < 0 || head_dense < 0) continue;
                    double head_dist = ctx->lower_bounds[
                        (((size_t)t * ctx->num_nodes) + head_dense) * ctx->local_dim + d];
                    if (!isfinite(head_dist)) continue;
                    double w = ctx->instance->arc_local_res[(size_t)a * ctx->local_dim + d];
                    double cand = head_dist + w;
                    double tail_dist = ctx->lower_bounds[
                        (((size_t)t * ctx->num_nodes) + tail_dense) * ctx->local_dim + d];
                    if (cand < tail_dist) {
                        ctx->lower_bounds[
                            (((size_t)t * ctx->num_nodes) + tail_dense) * ctx->local_dim + d
                        ] = cand;
                        updated = 1;
                    }
                }
                iter++;
            }
        }
    }
    return 0;
}

/* ---------- Result building ---------- */

static int sink_state_compare(const p1_ctx_t* ctx, const p1_state_t* a, const p1_state_t* b) {
    /* Python orders sink states by:
     *   (first_customer_sort_key, label.cost, global_resources, path) */
    int a_known = (a->first_customer >= 0);
    int b_known = (b->first_customer >= 0);
    if (a_known != b_known) return a_known ? -1 : 1;
    if (a_known) {
        if (a->first_customer < b->first_customer) return -1;
        if (a->first_customer > b->first_customer) return  1;
    }
    if (a->label.cost < b->label.cost) return -1;
    if (a->label.cost > b->label.cost) return  1;
    int c = vec_compare(a->global_resources, b->global_resources, ctx->global_dim);
    if (c != 0) return c;
    int min_len = a->path_length < b->path_length ? a->path_length : b->path_length;
    c = int_vec_compare(a->path, b->path, min_len);
    if (c != 0) return c;
    if (a->path_length < b->path_length) return -1;
    if (a->path_length > b->path_length) return  1;
    return 0;
}

static const p1_ctx_t* g_sink_ctx = NULL;
static int qsort_sink_cmp(const void* lhs, const void* rhs) {
    return sink_state_compare(
        g_sink_ctx, *(const p1_state_t* const*)lhs, *(const p1_state_t* const*)rhs);
}

static int build_result(
    p1_ctx_t* ctx, mespprc_phase1_result_t* result, mespprc_arena_t* result_arena
) {
    p1_bucket_t* sink_bucket = &ctx->buckets[ctx->sink_node];
    int n = sink_bucket->count;
    if (n == 0) return 0;
    /* Sort sink states. */
    p1_state_t** ordered = mespprc_arena_alloc(
        ctx->arena, sizeof(p1_state_t*) * (size_t)n, sizeof(void*));
    if (!ordered) return -1;
    memcpy(ordered, sink_bucket->items, sizeof(p1_state_t*) * (size_t)n);
    g_sink_ctx = ctx;
    qsort(ordered, (size_t)n, sizeof(p1_state_t*), qsort_sink_cmp);
    g_sink_ctx = NULL;

    /* Compute total path length to allocate paths array in one shot. */
    int total_path_len = 0;
    for (int i = 0; i < n; ++i) total_path_len += ordered[i]->path_length;

    result->route_count = n;
    result->costs = mespprc_arena_alloc(
        result_arena, sizeof(double) * (size_t)n, sizeof(double));
    result->path_offsets = mespprc_arena_alloc(
        result_arena, sizeof(int) * (size_t)(n + 1), sizeof(int));
    result->paths = mespprc_arena_alloc(
        result_arena, sizeof(int) * (size_t)total_path_len, sizeof(int));
    result->local_resources = ctx->local_dim > 0 ? mespprc_arena_alloc(
        result_arena, sizeof(double) * (size_t)n * ctx->local_dim, sizeof(double)) : NULL;
    result->global_resources = ctx->global_dim > 0 ? mespprc_arena_alloc(
        result_arena, sizeof(double) * (size_t)n * ctx->global_dim, sizeof(double)) : NULL;
    result->first_customers = mespprc_arena_alloc(
        result_arena, sizeof(int) * (size_t)n, sizeof(int));
    result->customer_state_sigs = ctx->num_customers > 0 ? mespprc_arena_alloc(
        result_arena, sizeof(int) * (size_t)n * ctx->num_customers, sizeof(int)) : NULL;
    if (!result->costs || !result->path_offsets || !result->paths
        || (ctx->local_dim > 0 && !result->local_resources)
        || (ctx->global_dim > 0 && !result->global_resources)
        || !result->first_customers
        || (ctx->num_customers > 0 && !result->customer_state_sigs)) {
        return -1;
    }

    int offset = 0;
    for (int i = 0; i < n; ++i) {
        const p1_state_t* st = ordered[i];
        result->costs[i] = st->label.cost;
        result->first_customers[i] = st->first_customer;
        result->path_offsets[i] = offset;
        /* Convert dense path to external node ids. */
        for (int p = 0; p < st->path_length; ++p) {
            result->paths[offset + p] = ctx->instance->nodes[st->path[p]].id;
        }
        offset += st->path_length;
        if (ctx->local_dim > 0) {
            memcpy(result->local_resources + (size_t)i * ctx->local_dim,
                   st->label.resources,
                   (size_t)ctx->local_dim * sizeof(double));
        }
        if (ctx->global_dim > 0) {
            memcpy(result->global_resources + (size_t)i * ctx->global_dim,
                   st->global_resources,
                   (size_t)ctx->global_dim * sizeof(double));
        }
        if (ctx->num_customers > 0) {
            memcpy(result->customer_state_sigs + (size_t)i * ctx->num_customers,
                   st->label.unreachable_vector,
                   (size_t)ctx->num_customers * sizeof(int));
        }
    }
    result->path_offsets[n] = offset;
    return 0;
}

/* ---------- Vector helpers ---------- */

static int vec_le(const double* a, const double* b, int n) {
    for (int i = 0; i < n; ++i) if (a[i] > b[i]) return 0;
    return 1;
}
static int vec_lt(const double* a, const double* b, int n) {
    int strict = 0;
    for (int i = 0; i < n; ++i) {
        if (a[i] > b[i]) return 0;
        if (a[i] < b[i]) strict = 1;
    }
    return strict;
}
static int vec_within_limits(const double* v, const double* limit, int n) {
    for (int i = 0; i < n; ++i) if (v[i] > limit[i] + 1e-12) return 0;
    return 1;
}
static void vec_add(const double* a, const double* b, double* out, int n) {
    for (int i = 0; i < n; ++i) out[i] = a[i] + b[i];
}
static int vec_compare(const double* a, const double* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}
static int int_vec_compare(const int* a, const int* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}
