/*
 * Phase 2 DP — route-network covering DP (C port).
 *
 * Mirrors mespprc/phase2_dp.py. The algorithm:
 *
 *   1. Take the Phase 1 result as the input route pool.
 *   2. Reduce the route pool by structural dominance.
 *   3. Build the route network (source -> route nodes -> sink, plus
 *      pairwise-compatible route -> route arcs).
 *   4. Run the labeling DP twice: relaxed (no global limits) then
 *      constrained (global limits enforced).
 *   5. Pick the best sink-state by (cost, resources, route_sequence).
 *
 * The customer-state semantics, refresh logic, dominance partial order, and
 * the route-pool reduction predicates all come from the Python implementation
 * verbatim. The structural-rank mapping for Phase 2 is:
 *   visited (>0)         -> rank 0
 *   reachable (0)        -> rank 1
 *   temp_unreachable (-2)-> rank 2
 *   perm_unreachable (-1)-> rank 3
 */

#include "internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define P2_REACHABLE          0
#define P2_TEMP_UNREACHABLE  -2
#define P2_PERM_UNREACHABLE  -1

#define P2_RANK_VISITED      0
#define P2_RANK_REACHABLE    1
#define P2_RANK_TEMP         2
#define P2_RANK_PERM         3

static inline int p2_is_visited(int s)          { return s > 0; }
static inline int p2_is_reachable(int s)        { return s == P2_REACHABLE; }
static inline int p2_is_temp_unreachable(int s) { return s == P2_TEMP_UNREACHABLE; }
static inline int p2_is_perm_unreachable(int s) { return s == P2_PERM_UNREACHABLE; }

static inline int p2_customer_state_rank(int s) {
    if (p2_is_visited(s))           return P2_RANK_VISITED;
    if (p2_is_reachable(s))         return P2_RANK_REACHABLE;
    if (p2_is_temp_unreachable(s))  return P2_RANK_TEMP;
    return P2_RANK_PERM;
}

/* ---------- Internal types ---------- */

typedef struct {
    int     phase1_index;            /* 0-based index into the Phase 1 result */
    int     route_id;                /* 1-based, matches Python convention */
    double  cost;
    double* global_resources;        /* global_dim */
    int*    required_covered;        /* sorted dense customer indices */
    int     required_covered_count;
    int     first_customer_in_route; /* external node id, -1 if None */
    int     full_signature_present;  /* 1 iff signature length == num_customers */
    int*    customer_state_signature;/* num_customers entries, full Phase 1 signature */
    int*    visit_sequence;          /* dense customer indices, route's visit order */
    int     visit_sequence_count;
    int*    structural_signature;    /* num_required_customers, ranks 0/1/2/3 */
    int     covered_bitset_first_bit_index;
    /* Optional metadata used in the canonical sort key. */
    int     all_covered_count;       /* count of nodes in path that are customers */
    int*    all_covered;             /* sorted external customer ids */
} p2_route_t;

typedef struct {
    int     n_route_nodes;
    int     source_node;
    int     sink_node;
    int     total_nodes;             /* sink_node + 1 */
    int*    succ_offset;             /* total_nodes + 1 */
    int*    succ_node;               /* total succ entries */
} p2_network_t;

typedef struct {
    int     current_node;
    double  cost;
    double* resources;               /* global_dim */
    int*    unreachable_vector;      /* num_required_customers */
    int*    sequence_structural_profile;  /* num_required_customers */
    int*    residual_structural_support;  /* num_required_customers */
    int*    route_sequence;          /* route_ids in order */
    int     route_sequence_length;
} p2_state_t;

typedef struct {
    p2_state_t** items;
    int          count;
    int          capacity;
} p2_bucket_t;

typedef struct {
    const mespprc_instance_t* instance;
    const mespprc_phase1_result_t* phase1;

    int     num_required_customers;
    int*    required_customer_ids;       /* external node ids, ordered */
    int*    required_idx_for_external;   /* max_external_id + 1 entries; -1 if not required */
    int     max_external_required_id;

    int     global_dim;

    /* Reduced route pool (post-dominance). */
    p2_route_t* routes;
    int         route_count;

    p2_network_t network;
    p2_bucket_t* buckets;            /* one per network node */

    int*    active_queue;
    int     active_count;
    int*    in_active_queue;

    int     enforce_global_limits;
    int     label_limit;             /* not user-exposed for now */

    mespprc_arena_t* arena;
} p2_ctx_t;

struct mespprc_phase2_dp_result {
    int     status;
    int     infeasibility_reason;
    int     is_feasible;
    int     coverage_complete;
    double  total_cost;

    int     selected_route_count;
    int*    selected_phase1_indices; /* 0-based into the Phase 1 result */
    int*    selected_route_ids;      /* 1-based, mirrors Python route_ids */

    mespprc_arena_t* arena;
};

/* ---------- Forward declarations ---------- */

static int   prepare_routes(p2_ctx_t* ctx);
static int   reduce_route_pool(p2_ctx_t* ctx);
static int   build_route_network(p2_ctx_t* ctx);
static int   solve_on_route_network(
    p2_ctx_t* ctx, int enforce_global_limits, p2_state_t** out_best);
static int   covers_all_required(const p2_ctx_t* ctx, const p2_state_t* state);
static int   route_is_exact_once_compatible(
    const p2_ctx_t* ctx, const p2_route_t* route, const int* vector);
static int   route_individually_global_feasible(
    const p2_ctx_t* ctx, const p2_route_t* route);

static int   try_extend_state(
    p2_ctx_t* ctx, const p2_state_t* state, int succ, p2_state_t** out);
static int   insert_with_dominance(
    p2_ctx_t* ctx, p2_bucket_t* bucket, p2_state_t* candidate);
static int   state_dominates(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b);
static int   states_equivalent(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b);
static int   label_dominates(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b);
static int   labels_equal(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b);
static int   p2_state_sort_compare(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b);

static void  refresh_customer_states(
    p2_ctx_t* ctx, int current_node, const double* global_resources, int* vector);
static int   classify_customer_state(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector);
static int   proves_permanent_unreachability(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector);
static int   can_currently_reach_customer(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector);
static int   can_still_reach_customer(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector);
static int   has_residual_support(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector,
    int require_globally_feasible);

static int   compute_residual_structural_support(
    p2_ctx_t* ctx, int current_node,
    const double* global_resources, const int* vector,
    int* out);
static int   best_residual_support_rank(
    p2_ctx_t* ctx, int target_cust_idx, int current_node,
    const double* global_resources, const int* vector);
static int   merge_sequence_structural_profile(
    const p2_ctx_t* ctx, const int* current_profile,
    const p2_route_t* route, int* out);

static int   bucket_grow(p2_ctx_t* ctx, p2_bucket_t* bucket, int min_capacity);
static int   bucket_append(p2_ctx_t* ctx, p2_bucket_t* bucket, p2_state_t* state);
static int   bucket_remove_at(p2_bucket_t* bucket, int index);
static void  bucket_sort(const p2_ctx_t* ctx, p2_bucket_t* bucket);

static void  active_push(p2_ctx_t* ctx, int node);
static int   active_pop(p2_ctx_t* ctx);

static int   vec_le(const double* a, const double* b, int n);
static int   vec_lt(const double* a, const double* b, int n);
static int   within_limits(const double* v, const double* limit, int n);
static int   vec_compare(const double* a, const double* b, int n);
static int   int_vec_compare(const int* a, const int* b, int n);
static int   sig_no_worse(const int* a, const int* b, int n);
static int   sig_strictly_better(const int* a, const int* b, int n);

static int   route_order_compare(
    const p2_ctx_t* ctx, const p2_route_t* a, const p2_route_t* b);
static int   pairwise_compatible(
    const p2_ctx_t* ctx, const p2_route_t* a, const p2_route_t* b);

static p2_state_t* clone_state(p2_ctx_t* ctx, const p2_state_t* src);
static p2_state_t* alloc_state(p2_ctx_t* ctx);
static int   alloc_state_buffers(p2_ctx_t* ctx, p2_state_t* st);

/* ---------- Public entry point ---------- */

mespprc_status_t mespprc_solve_phase2_dp(
    const mespprc_instance_t* instance,
    const mespprc_phase1_result_t* routes,
    mespprc_phase2_dp_result_t** out_result
) {
    if (!instance || !routes || !out_result) return MESPPRC_ERR_INVALID_ARG;
    if (!instance->finalized) return MESPPRC_ERR_NOT_FINALIZED;
    *out_result = NULL;

    mespprc_arena_t* scratch = mespprc_arena_create(0);
    if (!scratch) return MESPPRC_ERR_NOMEM;

    p2_ctx_t* ctx = mespprc_arena_calloc(scratch, sizeof(p2_ctx_t), 0);
    if (!ctx) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
    ctx->instance = instance;
    ctx->phase1 = routes;
    ctx->global_dim = instance->global_dim;
    ctx->arena = scratch;

    /* Build the required-customer index. The MESPPRCInstance currently treats
     * every customer as required (the Python `_explicit_required_customers`
     * default). We mirror that here. */
    int max_id = 0;
    int n_cust = 0;
    for (int v = 0; v < instance->num_nodes; ++v) {
        if (instance->nodes[v].type == MESPPRC_NODE_TYPE_CUSTOMER) {
            n_cust++;
            if (instance->nodes[v].id > max_id) max_id = instance->nodes[v].id;
        }
    }
    ctx->num_required_customers = n_cust;
    ctx->max_external_required_id = max_id;
    ctx->required_customer_ids =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)(n_cust > 0 ? n_cust : 1),
                            sizeof(int));
    ctx->required_idx_for_external =
        mespprc_arena_alloc(scratch, sizeof(int) * (size_t)(max_id + 1), sizeof(int));
    if (!ctx->required_customer_ids || !ctx->required_idx_for_external) {
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    for (int i = 0; i <= max_id; ++i) ctx->required_idx_for_external[i] = -1;
    /* Walk customers in id order to match Python's `instance.customers()` */
    int* sorted_cust_ids = mespprc_arena_alloc(scratch, sizeof(int) * (size_t)(n_cust > 0 ? n_cust : 1), sizeof(int));
    if (!sorted_cust_ids) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
    {
        int k = 0;
        for (int v = 0; v < instance->num_nodes; ++v) {
            if (instance->nodes[v].type == MESPPRC_NODE_TYPE_CUSTOMER) {
                sorted_cust_ids[k++] = instance->nodes[v].id;
            }
        }
        /* Sort by id ascending. */
        for (int i = 1; i < n_cust; ++i) {
            int cur = sorted_cust_ids[i];
            int j = i - 1;
            while (j >= 0 && sorted_cust_ids[j] > cur) {
                sorted_cust_ids[j + 1] = sorted_cust_ids[j];
                j--;
            }
            sorted_cust_ids[j + 1] = cur;
        }
        for (int i = 0; i < n_cust; ++i) {
            ctx->required_customer_ids[i] = sorted_cust_ids[i];
            ctx->required_idx_for_external[sorted_cust_ids[i]] = i;
        }
    }

    /* Step 1: prepare per-route metadata from the Phase 1 result. */
    if (prepare_routes(ctx) != 0) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }

    /* Step 2: route pool reduction. */
    if (reduce_route_pool(ctx) != 0) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }

    /* Result arena (kept alive after solver returns). */
    mespprc_arena_t* result_arena = mespprc_arena_create(0);
    if (!result_arena) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
    mespprc_phase2_dp_result_t* result =
        mespprc_arena_calloc(result_arena, sizeof(*result), 0);
    if (!result) {
        mespprc_arena_destroy(result_arena);
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    result->arena = result_arena;
    result->total_cost = NAN;

    /* Quick coverage check: do the reduced routes collectively cover every
     * required customer? Mirrors Python's diagnostics-based shortcut. */
    {
        int* covered_any = mespprc_arena_calloc(
            scratch, sizeof(int) * (size_t)(ctx->num_required_customers > 0 ? ctx->num_required_customers : 1),
            sizeof(int));
        if (!covered_any) { mespprc_arena_destroy(result_arena); mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
        for (int r = 0; r < ctx->route_count; ++r) {
            const p2_route_t* route = &ctx->routes[r];
            for (int k = 0; k < route->required_covered_count; ++k) {
                covered_any[route->required_covered[k]] = 1;
            }
        }
        for (int i = 0; i < ctx->num_required_customers; ++i) {
            if (!covered_any[i]) {
                result->status = MESPPRC_PHASE2_STATUS_INFEASIBLE;
                result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_ROUTE_SET_INFEASIBLE;
                result->is_feasible = 0;
                result->coverage_complete = 0;
                mespprc_arena_destroy(scratch);
                *out_result = result;
                return MESPPRC_OK;
            }
        }
    }

    /* Step 3: build route network. */
    if (build_route_network(ctx) != 0) {
        mespprc_arena_destroy(result_arena); mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }

    /* Step 4: solve relaxed (no global limits). If infeasible, the route set
     * cannot cover all required customers via any feasible combination. */
    p2_state_t* relaxed_best = NULL;
    int rc = solve_on_route_network(ctx, /*enforce_global=*/0, &relaxed_best);
    if (rc != 0) {
        mespprc_arena_destroy(result_arena); mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    if (!relaxed_best) {
        result->status = MESPPRC_PHASE2_STATUS_INFEASIBLE;
        result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_ROUTE_SET_INFEASIBLE;
        mespprc_arena_destroy(scratch);
        *out_result = result;
        return MESPPRC_OK;
    }

    /* Step 5: solve constrained (global limits enforced). */
    p2_state_t* constrained_best = NULL;
    rc = solve_on_route_network(ctx, /*enforce_global=*/1, &constrained_best);
    if (rc != 0) {
        mespprc_arena_destroy(result_arena); mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    if (!constrained_best) {
        result->status = MESPPRC_PHASE2_STATUS_INFEASIBLE;
        result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_GLOBAL_LIMITS;
        mespprc_arena_destroy(scratch);
        *out_result = result;
        return MESPPRC_OK;
    }

    /* Build the success result. */
    result->status = MESPPRC_PHASE2_STATUS_OPTIMAL;
    result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_NONE;
    result->is_feasible = 1;
    result->coverage_complete = 1;  /* sink-extension requires full coverage */
    result->total_cost = constrained_best->cost;
    result->selected_route_count = constrained_best->route_sequence_length;
    if (result->selected_route_count > 0) {
        result->selected_route_ids = mespprc_arena_alloc(
            result_arena, sizeof(int) * (size_t)result->selected_route_count, sizeof(int));
        result->selected_phase1_indices = mespprc_arena_alloc(
            result_arena, sizeof(int) * (size_t)result->selected_route_count, sizeof(int));
        if (!result->selected_route_ids || !result->selected_phase1_indices) {
            mespprc_arena_destroy(result_arena); mespprc_arena_destroy(scratch);
            return MESPPRC_ERR_NOMEM;
        }
        /* Map route_id to phase1_index using ctx->routes. */
        for (int i = 0; i < result->selected_route_count; ++i) {
            int rid = constrained_best->route_sequence[i];
            int phase1_idx = -1;
            for (int r = 0; r < ctx->route_count; ++r) {
                if (ctx->routes[r].route_id == rid) {
                    phase1_idx = ctx->routes[r].phase1_index;
                    break;
                }
            }
            result->selected_route_ids[i] = rid;
            result->selected_phase1_indices[i] = phase1_idx;
        }
    }

    mespprc_arena_destroy(scratch);
    *out_result = result;
    return MESPPRC_OK;
}

void mespprc_phase2_dp_result_destroy(mespprc_phase2_dp_result_t* result) {
    if (!result) return;
    mespprc_arena_t* arena = result->arena;
    mespprc_arena_destroy(arena);
}

/* ---------- Result accessors ---------- */

int mespprc_phase2_dp_status(const mespprc_phase2_dp_result_t* r) {
    return r ? r->status : MESPPRC_PHASE2_STATUS_INFEASIBLE;
}
int mespprc_phase2_dp_infeasibility_reason(const mespprc_phase2_dp_result_t* r) {
    return r ? r->infeasibility_reason : MESPPRC_PHASE2_INFEAS_NONE;
}
int mespprc_phase2_dp_is_feasible(const mespprc_phase2_dp_result_t* r) {
    return r ? r->is_feasible : 0;
}
int mespprc_phase2_dp_coverage_complete(const mespprc_phase2_dp_result_t* r) {
    return r ? r->coverage_complete : 0;
}
mespprc_status_t mespprc_phase2_dp_total_cost(
    const mespprc_phase2_dp_result_t* r, double* out_cost
) {
    if (!r || !out_cost) return MESPPRC_ERR_INVALID_ARG;
    if (!r->is_feasible) return MESPPRC_ERR_INSTANCE_INVALID;
    *out_cost = r->total_cost;
    return MESPPRC_OK;
}
int mespprc_phase2_dp_selected_route_count(const mespprc_phase2_dp_result_t* r) {
    return r ? r->selected_route_count : 0;
}
mespprc_status_t mespprc_phase2_dp_selected_routes(
    const mespprc_phase2_dp_result_t* r, int* buf, int cap
) {
    if (!r || !buf) return MESPPRC_ERR_INVALID_ARG;
    if (cap < r->selected_route_count) return MESPPRC_ERR_BUFFER_TOO_SMALL;
    if (r->selected_route_count > 0) {
        memcpy(buf, r->selected_phase1_indices,
               (size_t)r->selected_route_count * sizeof(int));
    }
    return MESPPRC_OK;
}

/* ---------- Route preparation ---------- */

static int int_compare_qsort(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

static int prepare_routes(p2_ctx_t* ctx) {
    int n = ctx->phase1->route_count;
    int n_all_customers = ctx->phase1->num_customers;
    int n_required = ctx->num_required_customers;

    ctx->routes = n > 0
        ? mespprc_arena_calloc(ctx->arena, sizeof(p2_route_t) * (size_t)n, sizeof(p2_route_t))
        : NULL;
    if (n > 0 && !ctx->routes) return -1;
    ctx->route_count = n;

    for (int i = 0; i < n; ++i) {
        p2_route_t* r = &ctx->routes[i];
        r->phase1_index = i;
        r->route_id = i + 1;            /* match Python's 1-based ids */
        r->cost = ctx->phase1->costs[i];
        if (ctx->global_dim > 0) {
            r->global_resources = mespprc_arena_alloc(
                ctx->arena, sizeof(double) * (size_t)ctx->global_dim, sizeof(double));
            if (!r->global_resources) return -1;
            memcpy(r->global_resources,
                   ctx->phase1->global_resources + (size_t)i * ctx->global_dim,
                   (size_t)ctx->global_dim * sizeof(double));
        }
        r->first_customer_in_route = ctx->phase1->first_customers[i];

        /* Phase 1's customer_state_signature has length num_customers (all customers).
         * The full-signature flag is true when length matches, which it always does
         * here because that's exactly what the Phase 1 port emits. */
        r->full_signature_present = (n_all_customers > 0) ? 1 : 0;
        r->customer_state_signature = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)(n_all_customers > 0 ? n_all_customers : 1),
            sizeof(int));
        if (!r->customer_state_signature) return -1;
        if (n_all_customers > 0) {
            memcpy(r->customer_state_signature,
                   ctx->phase1->customer_state_sigs + (size_t)i * n_all_customers,
                   (size_t)n_all_customers * sizeof(int));
        }

        /* Compute required_covered (sorted dense indices), structural_signature
         * (length = num_required_customers, ranks 0/1/2/3), all_covered list,
         * and visit_sequence (visit order along path). */
        int* req_buf = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)(n_required > 0 ? n_required : 1),
            sizeof(int));
        if (!req_buf) return -1;
        int req_count = 0;
        for (int c = 0; c < n_required; ++c) {
            int signature_value = (c < n_all_customers) ? r->customer_state_signature[c] : 0;
            if (p2_is_visited(signature_value)) {
                req_buf[req_count++] = c;
            }
        }
        r->required_covered = req_buf;
        r->required_covered_count = req_count;

        r->structural_signature = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)(n_required > 0 ? n_required : 1),
            sizeof(int));
        if (!r->structural_signature) return -1;
        for (int c = 0; c < n_required; ++c) {
            int sig_val = (c < n_all_customers) ? r->customer_state_signature[c] : P2_TEMP_UNREACHABLE;
            r->structural_signature[c] = p2_customer_state_rank(sig_val);
        }

        /* visit_sequence: walk the path in order, append customer indices not yet seen. */
        int path_len = ctx->phase1->path_offsets[i + 1] - ctx->phase1->path_offsets[i];
        const int* path_ext = ctx->phase1->paths + ctx->phase1->path_offsets[i];
        int* visit = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)(req_count > 0 ? req_count : 1), sizeof(int));
        int* in_visit = mespprc_arena_calloc(
            ctx->arena, sizeof(int) * (size_t)(req_count > 0 ? req_count : 1), sizeof(int));
        if (!visit || !in_visit) return -1;
        int visit_count = 0;
        for (int p = 0; p < path_len; ++p) {
            int ext_id = path_ext[p];
            if (ext_id < 0 || ext_id > ctx->max_external_required_id) continue;
            int dense = ctx->required_idx_for_external[ext_id];
            if (dense < 0) continue;
            /* This customer is required; check it appears in required_covered for this route. */
            int found_in_required = 0;
            for (int k = 0; k < req_count; ++k) {
                if (req_buf[k] == dense) {
                    found_in_required = 1;
                    if (!in_visit[k]) {
                        visit[visit_count++] = dense;
                        in_visit[k] = 1;
                    }
                    break;
                }
            }
            (void)found_in_required;
        }
        /* Append any remaining required-covered customers in id order. */
        for (int k = 0; k < req_count; ++k) {
            if (!in_visit[k]) {
                visit[visit_count++] = req_buf[k];
                in_visit[k] = 1;
            }
        }
        r->visit_sequence = visit;
        r->visit_sequence_count = visit_count;

        /* all_covered: external customer ids in path that are customers, sorted. */
        int* ac_buf = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)(path_len > 0 ? path_len : 1), sizeof(int));
        if (!ac_buf) return -1;
        int ac_count = 0;
        for (int p = 0; p < path_len; ++p) {
            int ext_id = path_ext[p];
            int dense_idx = mespprc_instance_node_index(ctx->instance, ext_id);
            if (dense_idx < 0) continue;
            if (ctx->instance->nodes[dense_idx].type != MESPPRC_NODE_TYPE_CUSTOMER) continue;
            /* Avoid duplicates. */
            int dup = 0;
            for (int q = 0; q < ac_count; ++q) {
                if (ac_buf[q] == ext_id) { dup = 1; break; }
            }
            if (!dup) ac_buf[ac_count++] = ext_id;
        }
        qsort(ac_buf, (size_t)ac_count, sizeof(int), int_compare_qsort);
        r->all_covered = ac_buf;
        r->all_covered_count = ac_count;
    }
    return 0;
}

/* ---------- Route order key (mirrors Python `_route_order_key`) ---------- */

static int route_order_compare(
    const p2_ctx_t* ctx, const p2_route_t* a, const p2_route_t* b
) {
    /* (1) first_customer_sort_key */
    int a_known = (a->first_customer_in_route >= 0);
    int b_known = (b->first_customer_in_route >= 0);
    if (a_known != b_known) return a_known ? -1 : 1;
    if (a_known) {
        if (a->first_customer_in_route < b->first_customer_in_route) return -1;
        if (a->first_customer_in_route > b->first_customer_in_route) return  1;
    }

    /* (2) sorted required_covered (external ids) */
    {
        int min_len = a->required_covered_count < b->required_covered_count
            ? a->required_covered_count : b->required_covered_count;
        for (int k = 0; k < min_len; ++k) {
            int aid = ctx->required_customer_ids[a->required_covered[k]];
            int bid = ctx->required_customer_ids[b->required_covered[k]];
            if (aid < bid) return -1;
            if (aid > bid) return  1;
        }
        if (a->required_covered_count < b->required_covered_count) return -1;
        if (a->required_covered_count > b->required_covered_count) return  1;
    }
    /* (3) sorted all_covered */
    {
        int min_len = a->all_covered_count < b->all_covered_count
            ? a->all_covered_count : b->all_covered_count;
        for (int k = 0; k < min_len; ++k) {
            if (a->all_covered[k] < b->all_covered[k]) return -1;
            if (a->all_covered[k] > b->all_covered[k]) return  1;
        }
        if (a->all_covered_count < b->all_covered_count) return -1;
        if (a->all_covered_count > b->all_covered_count) return  1;
    }
    /* (4) structural_signature (required-only) */
    {
        int n = ctx->num_required_customers;
        int c = int_vec_compare(a->structural_signature, b->structural_signature, n);
        if (c != 0) return c;
    }
    /* (5) collapsed customer_state_signature (visited -> 1, else value) */
    {
        int n_all = ctx->phase1->num_customers;
        for (int k = 0; k < n_all; ++k) {
            int av = a->customer_state_signature[k];
            int bv = b->customer_state_signature[k];
            int ac = p2_is_visited(av) ? 1 : av;
            int bc = p2_is_visited(bv) ? 1 : bv;
            if (ac < bc) return -1;
            if (ac > bc) return  1;
        }
    }
    /* (6) cost */
    if (a->cost < b->cost) return -1;
    if (a->cost > b->cost) return  1;
    /* (7) global_resources */
    {
        int c = vec_compare(a->global_resources, b->global_resources, ctx->global_dim);
        if (c != 0) return c;
    }
    /* (8) path — we don't have ready access to the dense path here, but the
     * canonical key is unique up to point (5)+cost+resources for our
     * generated instances. As a final tiebreak we use route_id. */
    if (a->route_id < b->route_id) return -1;
    if (a->route_id > b->route_id) return  1;
    return 0;
}

/* ---------- Route pool reduction ---------- */

static int route_no_worse_global(const p2_ctx_t* ctx, const p2_route_t* a, const p2_route_t* b) {
    return vec_le(a->global_resources, b->global_resources, ctx->global_dim);
}

static int route_structurally_dominates(
    const p2_ctx_t* ctx, const p2_route_t* a, const p2_route_t* b
) {
    /* Same required_covered. */
    if (a->required_covered_count != b->required_covered_count) return 0;
    if (int_vec_compare(a->required_covered, b->required_covered,
                        a->required_covered_count) != 0) return 0;
    if (a->first_customer_in_route != b->first_customer_in_route) return 0;
    if (!route_no_worse_global(ctx, a, b)) return 0;
    if (a->cost > b->cost) return 0;
    if (!a->full_signature_present || !b->full_signature_present) return 0;
    if (!sig_no_worse(a->structural_signature, b->structural_signature,
                      ctx->num_required_customers)) return 0;
    int strict = (a->cost < b->cost)
        || vec_lt(a->global_resources, b->global_resources, ctx->global_dim)
        || sig_strictly_better(a->structural_signature, b->structural_signature,
                               ctx->num_required_customers);
    return strict;
}

static int reduce_route_pool(p2_ctx_t* ctx) {
    int n = ctx->route_count;
    if (n == 0) return 0;

    /* Group by (sorted_required_covered, first_customer_in_route). For each
     * group, do pairwise dominance filtering. */
    int* group_id = mespprc_arena_alloc(ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    int* visited = mespprc_arena_calloc(ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    if (!group_id || !visited) return -1;
    int next_group = 0;
    for (int i = 0; i < n; ++i) {
        if (visited[i]) continue;
        group_id[i] = next_group;
        visited[i] = 1;
        for (int j = i + 1; j < n; ++j) {
            if (visited[j]) continue;
            const p2_route_t* a = &ctx->routes[i];
            const p2_route_t* b = &ctx->routes[j];
            if (a->required_covered_count != b->required_covered_count) continue;
            if (int_vec_compare(a->required_covered, b->required_covered,
                                a->required_covered_count) != 0) continue;
            if (a->first_customer_in_route != b->first_customer_in_route) continue;
            group_id[j] = next_group;
            visited[j] = 1;
        }
        next_group++;
    }

    int* keep = mespprc_arena_alloc(ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    if (!keep) return -1;
    for (int i = 0; i < n; ++i) keep[i] = 1;

    /* For each group, collect indices, sort by route_order_key, run pairwise
     * dominance. Survivors only. */
    for (int g = 0; g < next_group; ++g) {
        int* members = mespprc_arena_alloc(ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
        if (!members) return -1;
        int m = 0;
        for (int i = 0; i < n; ++i) {
            if (group_id[i] == g) members[m++] = i;
        }
        /* Insertion sort by route_order_compare. */
        for (int i = 1; i < m; ++i) {
            int cur = members[i];
            int j = i - 1;
            while (j >= 0 && route_order_compare(ctx, &ctx->routes[members[j]], &ctx->routes[cur]) > 0) {
                members[j + 1] = members[j];
                j--;
            }
            members[j + 1] = cur;
        }
        int* survivor_idx = mespprc_arena_alloc(ctx->arena, sizeof(int) * (size_t)m, sizeof(int));
        if (!survivor_idx) return -1;
        int survivor_count = 0;
        for (int k = 0; k < m; ++k) {
            int route_idx = members[k];
            const p2_route_t* route = &ctx->routes[route_idx];
            int dominated = 0;
            int new_survivor_count = 0;
            for (int s = 0; s < survivor_count; ++s) {
                int sidx = survivor_idx[s];
                const p2_route_t* survivor = &ctx->routes[sidx];
                if (route_structurally_dominates(ctx, survivor, route)) {
                    dominated = 1;
                    break;
                }
                if (route_structurally_dominates(ctx, route, survivor)) {
                    keep[sidx] = 0;
                    continue;
                }
                survivor_idx[new_survivor_count++] = sidx;
            }
            survivor_count = new_survivor_count;
            if (!dominated) {
                survivor_idx[survivor_count++] = route_idx;
            } else {
                keep[route_idx] = 0;
            }
        }
    }

    /* Compact ctx->routes in canonical order. */
    int new_n = 0;
    for (int i = 0; i < n; ++i) if (keep[i]) new_n++;
    if (new_n == 0) {
        ctx->route_count = 0;
        return 0;
    }
    p2_route_t* new_routes = mespprc_arena_alloc(
        ctx->arena, sizeof(p2_route_t) * (size_t)new_n, sizeof(p2_route_t));
    if (!new_routes) return -1;
    int k = 0;
    for (int i = 0; i < n; ++i) if (keep[i]) new_routes[k++] = ctx->routes[i];
    /* Sort by route_order_compare. */
    for (int i = 1; i < new_n; ++i) {
        p2_route_t cur = new_routes[i];
        int j = i - 1;
        while (j >= 0 && route_order_compare(ctx, &new_routes[j], &cur) > 0) {
            new_routes[j + 1] = new_routes[j];
            j--;
        }
        new_routes[j + 1] = cur;
    }
    ctx->routes = new_routes;
    ctx->route_count = new_n;
    return 0;
}

/* ---------- Route network ---------- */

static int pairwise_compatible(
    const p2_ctx_t* ctx, const p2_route_t* a, const p2_route_t* b
) {
    /* Required-covered sets are sorted dense ids; check disjoint via merge. */
    int i = 0, j = 0;
    while (i < a->required_covered_count && j < b->required_covered_count) {
        if (a->required_covered[i] == b->required_covered[j]) return 0;
        if (a->required_covered[i] < b->required_covered[j]) i++; else j++;
    }
    (void)ctx;
    return 1;
}

static int build_route_network(p2_ctx_t* ctx) {
    int n = ctx->route_count;
    int total_nodes = n + 2;
    ctx->network.n_route_nodes = n;
    ctx->network.source_node = 0;
    ctx->network.sink_node = n + 1;
    ctx->network.total_nodes = total_nodes;

    /* Count successors per node:
     *  source (0):        sink + every route node
     *  route node i:      sink + every j>i compatible with i
     *  sink:              none
     */
    ctx->network.succ_offset = mespprc_arena_calloc(
        ctx->arena, sizeof(int) * (size_t)(total_nodes + 1), sizeof(int));
    if (!ctx->network.succ_offset) return -1;

    /* succ_offset[i+1] holds count of successors of node i; we'll prefix-sum. */
    int total_succ = 0;
    /* source -> sink + n route nodes */
    ctx->network.succ_offset[1] = 1 + n;
    total_succ += 1 + n;
    /* each route node i: 1 (sink) + count of compatible j>i */
    for (int i = 0; i < n; ++i) {
        int count = 1;  /* sink */
        for (int j = i + 1; j < n; ++j) {
            if (pairwise_compatible(ctx, &ctx->routes[i], &ctx->routes[j])) count++;
        }
        ctx->network.succ_offset[(i + 1) + 1] = count;
        total_succ += count;
    }
    /* sink: 0 */
    ctx->network.succ_offset[(n + 1) + 1] = 0;

    /* Prefix-sum to make succ_offset cumulative. */
    for (int i = 1; i <= total_nodes; ++i) {
        ctx->network.succ_offset[i] += ctx->network.succ_offset[i - 1];
    }

    ctx->network.succ_node = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)(total_succ > 0 ? total_succ : 1), sizeof(int));
    if (!ctx->network.succ_node) return -1;

    /* Fill successors. */
    int offs;
    /* source */
    offs = ctx->network.succ_offset[0];
    ctx->network.succ_node[offs++] = ctx->network.sink_node;
    for (int i = 0; i < n; ++i) ctx->network.succ_node[offs++] = i + 1;

    /* route nodes */
    for (int i = 0; i < n; ++i) {
        int node = i + 1;
        offs = ctx->network.succ_offset[node];
        ctx->network.succ_node[offs++] = ctx->network.sink_node;
        for (int j = i + 1; j < n; ++j) {
            if (pairwise_compatible(ctx, &ctx->routes[i], &ctx->routes[j])) {
                ctx->network.succ_node[offs++] = j + 1;
            }
        }
    }
    return 0;
}

/* ---------- Solver core ---------- */

static int solve_on_route_network(
    p2_ctx_t* ctx, int enforce_global_limits, p2_state_t** out_best
) {
    ctx->enforce_global_limits = enforce_global_limits;
    *out_best = NULL;

    int total_nodes = ctx->network.total_nodes;
    /* Reset buckets and active queue. */
    ctx->buckets = mespprc_arena_calloc(
        ctx->arena, sizeof(p2_bucket_t) * (size_t)total_nodes, sizeof(p2_bucket_t));
    ctx->active_queue = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)total_nodes, sizeof(int));
    ctx->in_active_queue = mespprc_arena_calloc(
        ctx->arena, sizeof(int) * (size_t)total_nodes, sizeof(int));
    if (!ctx->buckets || !ctx->active_queue || !ctx->in_active_queue) return -1;
    ctx->active_count = 0;

    /* Build initial state at source. */
    p2_state_t* initial = alloc_state(ctx);
    if (!initial) return -1;
    initial->current_node = ctx->network.source_node;
    initial->cost = 0.0;
    if (ctx->global_dim > 0) {
        for (int d = 0; d < ctx->global_dim; ++d) initial->resources[d] = 0.0;
    }
    /* unreachable_vector: REACHABLE for required customers. */
    for (int i = 0; i < ctx->num_required_customers; ++i) {
        initial->unreachable_vector[i] = P2_REACHABLE;
    }
    refresh_customer_states(
        ctx, ctx->network.source_node, initial->resources, initial->unreachable_vector);
    /* sequence_structural_profile starts as PERM_UNREACHABLE rank for every customer */
    for (int i = 0; i < ctx->num_required_customers; ++i) {
        initial->sequence_structural_profile[i] = P2_RANK_PERM;
    }
    if (compute_residual_structural_support(
            ctx, ctx->network.source_node, initial->resources,
            initial->unreachable_vector, initial->residual_structural_support) != 0) return -1;

    if (bucket_append(ctx, &ctx->buckets[ctx->network.source_node], initial) != 0) return -1;
    active_push(ctx, ctx->network.source_node);

    /* Main loop. */
    while (ctx->active_count > 0) {
        int node = active_pop(ctx);
        if (node == ctx->network.sink_node) continue;
        p2_bucket_t* bucket = &ctx->buckets[node];
        if (bucket->count == 0) continue;
        bucket_sort(ctx, bucket);

        int snapshot_count = bucket->count;
        p2_state_t** snapshot = mespprc_arena_alloc(
            ctx->arena, sizeof(p2_state_t*) * (size_t)snapshot_count, sizeof(void*));
        if (!snapshot) return -1;
        memcpy(snapshot, bucket->items, sizeof(p2_state_t*) * (size_t)snapshot_count);

        int s_off = ctx->network.succ_offset[node];
        int s_end = ctx->network.succ_offset[node + 1];
        for (int s = 0; s < snapshot_count; ++s) {
            const p2_state_t* state = snapshot[s];
            for (int e = s_off; e < s_end; ++e) {
                int succ = ctx->network.succ_node[e];
                p2_state_t* extended = NULL;
                int rc = try_extend_state(ctx, state, succ, &extended);
                if (rc != 0) return -1;
                if (!extended) continue;
                int changed = insert_with_dominance(ctx, &ctx->buckets[succ], extended);
                if (changed) active_push(ctx, succ);
            }
        }
    }

    /* Pick best sink state. */
    p2_bucket_t* sink_bucket = &ctx->buckets[ctx->network.sink_node];
    if (sink_bucket->count == 0) return 0;
    p2_state_t* best = sink_bucket->items[0];
    for (int i = 1; i < sink_bucket->count; ++i) {
        const p2_state_t* candidate = sink_bucket->items[i];
        if (candidate->cost < best->cost) {
            best = sink_bucket->items[i];
            continue;
        }
        if (candidate->cost > best->cost) continue;
        int c = vec_compare(candidate->resources, best->resources, ctx->global_dim);
        if (c < 0) {
            best = sink_bucket->items[i];
            continue;
        }
        if (c > 0) continue;
        /* Tiebreak by route_sequence (lexicographic). */
        int min_len = candidate->route_sequence_length < best->route_sequence_length
            ? candidate->route_sequence_length : best->route_sequence_length;
        int seq_cmp = int_vec_compare(candidate->route_sequence, best->route_sequence, min_len);
        if (seq_cmp < 0
            || (seq_cmp == 0 && candidate->route_sequence_length < best->route_sequence_length)) {
            best = sink_bucket->items[i];
        }
    }
    *out_best = best;
    return 0;
}

/* ---------- State allocation ---------- */

static p2_state_t* alloc_state(p2_ctx_t* ctx) {
    p2_state_t* st = mespprc_arena_calloc(ctx->arena, sizeof(p2_state_t), sizeof(double));
    if (!st) return NULL;
    if (alloc_state_buffers(ctx, st) != 0) return NULL;
    return st;
}

static int alloc_state_buffers(p2_ctx_t* ctx, p2_state_t* st) {
    if (ctx->global_dim > 0) {
        st->resources = mespprc_arena_calloc(
            ctx->arena, sizeof(double) * (size_t)ctx->global_dim, sizeof(double));
        if (!st->resources) return -1;
    }
    int n = ctx->num_required_customers > 0 ? ctx->num_required_customers : 1;
    st->unreachable_vector = mespprc_arena_calloc(
        ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    st->sequence_structural_profile = mespprc_arena_calloc(
        ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    st->residual_structural_support = mespprc_arena_calloc(
        ctx->arena, sizeof(int) * (size_t)n, sizeof(int));
    if (!st->unreachable_vector || !st->sequence_structural_profile
        || !st->residual_structural_support) return -1;
    return 0;
}

static p2_state_t* clone_state(p2_ctx_t* ctx, const p2_state_t* src) {
    p2_state_t* st = alloc_state(ctx);
    if (!st) return NULL;
    st->current_node = src->current_node;
    st->cost = src->cost;
    if (ctx->global_dim > 0) {
        memcpy(st->resources, src->resources, sizeof(double) * (size_t)ctx->global_dim);
    }
    int n = ctx->num_required_customers;
    if (n > 0) {
        memcpy(st->unreachable_vector, src->unreachable_vector, sizeof(int) * (size_t)n);
        memcpy(st->sequence_structural_profile, src->sequence_structural_profile,
               sizeof(int) * (size_t)n);
        memcpy(st->residual_structural_support, src->residual_structural_support,
               sizeof(int) * (size_t)n);
    }
    if (src->route_sequence_length > 0) {
        st->route_sequence = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)src->route_sequence_length, sizeof(int));
        if (!st->route_sequence) return NULL;
        memcpy(st->route_sequence, src->route_sequence,
               sizeof(int) * (size_t)src->route_sequence_length);
        st->route_sequence_length = src->route_sequence_length;
    }
    return st;
}

/* ---------- State extension ---------- */

static int try_extend_state(
    p2_ctx_t* ctx, const p2_state_t* state, int succ, p2_state_t** out
) {
    *out = NULL;
    if (succ == ctx->network.sink_node) {
        if (!covers_all_required(ctx, state)) return 0;
        p2_state_t* st = clone_state(ctx, state);
        if (!st) return -1;
        st->current_node = succ;
        *out = st;
        return 0;
    }

    int route_idx = succ - 1;  /* route nodes are 1..n */
    if (route_idx < 0 || route_idx >= ctx->route_count) return 0;
    const p2_route_t* route = &ctx->routes[route_idx];

    if (!route_is_exact_once_compatible(ctx, route, state->unreachable_vector)) return 0;

    /* Compute new resources. */
    double* new_resources = NULL;
    if (ctx->global_dim > 0) {
        new_resources = mespprc_arena_alloc(
            ctx->arena, sizeof(double) * (size_t)ctx->global_dim, sizeof(double));
        if (!new_resources) return -1;
        for (int d = 0; d < ctx->global_dim; ++d) {
            new_resources[d] = state->resources[d] + route->global_resources[d];
            if (ctx->enforce_global_limits
                && new_resources[d] > ctx->instance->global_limits[d] + 1e-12) {
                return 0;
            }
        }
    }

    /* Update unreachable_vector with route's visit sequence. */
    int n = ctx->num_required_customers;
    int* new_vector = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)(n > 0 ? n : 1), sizeof(int));
    if (!new_vector) return -1;
    if (n > 0) memcpy(new_vector, state->unreachable_vector, sizeof(int) * (size_t)n);

    /* Compute next visit position. */
    int next_pos = 1;
    for (int i = 0; i < n; ++i) {
        if (new_vector[i] >= next_pos) next_pos = new_vector[i] + 1;
    }
    for (int k = 0; k < route->visit_sequence_count; ++k) {
        int dense = route->visit_sequence[k];
        if (dense < 0 || dense >= n) continue;
        if (p2_is_visited(new_vector[dense])) return 0;
        new_vector[dense] = next_pos++;
    }

    refresh_customer_states(ctx, succ, new_resources ? new_resources : state->resources, new_vector);

    /* Build new state. */
    p2_state_t* st = alloc_state(ctx);
    if (!st) return -1;
    st->current_node = succ;
    st->cost = state->cost + route->cost;
    if (ctx->global_dim > 0) {
        memcpy(st->resources, new_resources, sizeof(double) * (size_t)ctx->global_dim);
    }
    if (n > 0) memcpy(st->unreachable_vector, new_vector, sizeof(int) * (size_t)n);

    if (merge_sequence_structural_profile(
            ctx, state->sequence_structural_profile, route, st->sequence_structural_profile) != 0)
        return -1;
    if (compute_residual_structural_support(
            ctx, succ, st->resources, st->unreachable_vector,
            st->residual_structural_support) != 0) return -1;

    /* Append route_id to route_sequence. */
    st->route_sequence_length = state->route_sequence_length + 1;
    st->route_sequence = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)st->route_sequence_length, sizeof(int));
    if (!st->route_sequence) return -1;
    if (state->route_sequence_length > 0) {
        memcpy(st->route_sequence, state->route_sequence,
               sizeof(int) * (size_t)state->route_sequence_length);
    }
    st->route_sequence[state->route_sequence_length] = route->route_id;
    *out = st;
    return 0;
}

static int covers_all_required(const p2_ctx_t* ctx, const p2_state_t* state) {
    for (int i = 0; i < ctx->num_required_customers; ++i) {
        if (!p2_is_visited(state->unreachable_vector[i])) return 0;
    }
    return 1;
}

static int route_is_exact_once_compatible(
    const p2_ctx_t* ctx, const p2_route_t* route, const int* vector
) {
    (void)ctx;
    for (int k = 0; k < route->required_covered_count; ++k) {
        if (p2_is_visited(vector[route->required_covered[k]])) return 0;
    }
    return 1;
}

static int route_individually_global_feasible(
    const p2_ctx_t* ctx, const p2_route_t* route
) {
    if (ctx->global_dim == 0) return 1;
    return within_limits(route->global_resources, ctx->instance->global_limits, ctx->global_dim);
}

/* ---------- Customer state refresh ---------- */

static void refresh_customer_states(
    p2_ctx_t* ctx, int current_node, const double* global_resources, int* vector
) {
    int n = ctx->num_required_customers;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < n; ++i) {
            int v = vector[i];
            if (p2_is_visited(v) || p2_is_perm_unreachable(v)) continue;
            if (proves_permanent_unreachability(ctx, i, current_node, global_resources, vector)) {
                vector[i] = P2_PERM_UNREACHABLE;
                changed = 1;
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        int v = vector[i];
        if (p2_is_visited(v) || p2_is_perm_unreachable(v)) continue;
        vector[i] = classify_customer_state(ctx, i, current_node, global_resources, vector);
    }
}

static int proves_permanent_unreachability(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector
) {
    return !can_still_reach_customer(ctx, cust_idx, current_node, global_resources, vector);
}

static int classify_customer_state(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector
) {
    if (can_currently_reach_customer(ctx, cust_idx, current_node, global_resources, vector)) {
        return P2_REACHABLE;
    }
    return P2_TEMP_UNREACHABLE;
}

static int can_currently_reach_customer(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector
) {
    return has_residual_support(
        ctx, cust_idx, current_node, global_resources, vector,
        /*require_globally_feasible=*/1);
}

static int can_still_reach_customer(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector
) {
    return has_residual_support(
        ctx, cust_idx, current_node, global_resources, vector,
        /*require_globally_feasible=*/0);
}

static int has_residual_support(
    p2_ctx_t* ctx, int cust_idx, int current_node,
    const double* global_resources, const int* vector,
    int require_globally_feasible
) {
    int s_off = ctx->network.succ_offset[current_node];
    int s_end = ctx->network.succ_offset[current_node + 1];
    for (int e = s_off; e < s_end; ++e) {
        int succ = ctx->network.succ_node[e];
        if (succ == ctx->network.sink_node) continue;
        const p2_route_t* route = &ctx->routes[succ - 1];
        /* Must cover this customer. */
        int covers = 0;
        for (int k = 0; k < route->required_covered_count; ++k) {
            if (route->required_covered[k] == cust_idx) { covers = 1; break; }
        }
        if (!covers) continue;
        if (!route_is_exact_once_compatible(ctx, route, vector)) continue;
        if (require_globally_feasible && ctx->enforce_global_limits && ctx->global_dim > 0) {
            int feasible = 1;
            for (int d = 0; d < ctx->global_dim; ++d) {
                if (global_resources[d] + route->global_resources[d]
                    > ctx->instance->global_limits[d] + 1e-12) { feasible = 0; break; }
            }
            if (!feasible) continue;
        }
        return 1;
    }
    return 0;
}

/* ---------- Structural-support computation ---------- */

static int compute_residual_structural_support(
    p2_ctx_t* ctx, int current_node,
    const double* global_resources, const int* vector,
    int* out
) {
    int n = ctx->num_required_customers;
    for (int i = 0; i < n; ++i) {
        if (p2_is_visited(vector[i])) {
            out[i] = 0;
            continue;
        }
        out[i] = best_residual_support_rank(
            ctx, i, current_node, global_resources, vector);
    }
    return 0;
}

static int best_residual_support_rank(
    p2_ctx_t* ctx, int target_cust_idx, int current_node,
    const double* global_resources, const int* vector
) {
    int best = P2_RANK_PERM;
    int s_off = ctx->network.succ_offset[current_node];
    int s_end = ctx->network.succ_offset[current_node + 1];
    for (int e = s_off; e < s_end; ++e) {
        int succ = ctx->network.succ_node[e];
        if (succ == ctx->network.sink_node) continue;
        const p2_route_t* route = &ctx->routes[succ - 1];
        if (!route_is_exact_once_compatible(ctx, route, vector)) continue;
        if (ctx->enforce_global_limits && ctx->global_dim > 0) {
            int feasible = 1;
            for (int d = 0; d < ctx->global_dim; ++d) {
                if (global_resources[d] + route->global_resources[d]
                    > ctx->instance->global_limits[d] + 1e-12) { feasible = 0; break; }
            }
            if (!feasible) continue;
        }
        int candidate;
        int covers = 0;
        for (int k = 0; k < route->required_covered_count; ++k) {
            if (route->required_covered[k] == target_cust_idx) { covers = 1; break; }
        }
        if (covers) {
            candidate = 0;
        } else {
            candidate = route->structural_signature[target_cust_idx];
        }
        if (candidate < best) best = candidate;
    }
    return best;
}

static int merge_sequence_structural_profile(
    const p2_ctx_t* ctx, const int* current_profile,
    const p2_route_t* route, int* out
) {
    int n = ctx->num_required_customers;
    /* Python: if current_profile is empty (None), return route_profile.
     * In our port the initial state's profile is all PERM_UNREACHABLE rank,
     * which is the maximum, so element-wise min naturally yields the route's
     * profile on the first merge. */
    for (int i = 0; i < n; ++i) {
        int a = current_profile[i];
        int b = route->structural_signature[i];
        out[i] = a < b ? a : b;
    }
    return 0;
}

/* ---------- Dominance, equivalence, sort ---------- */

static int p2_state_sort_compare(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b
) {
    if (a->cost < b->cost) return -1;
    if (a->cost > b->cost) return  1;
    int c = vec_compare(a->resources, b->resources, ctx->global_dim);
    if (c != 0) return c;
    /* customer state signature (rank) */
    int n = ctx->num_required_customers;
    for (int i = 0; i < n; ++i) {
        int ar = p2_customer_state_rank(a->unreachable_vector[i]);
        int br = p2_customer_state_rank(b->unreachable_vector[i]);
        if (ar < br) return -1;
        if (ar > br) return  1;
    }
    c = int_vec_compare(a->sequence_structural_profile, b->sequence_structural_profile, n);
    if (c != 0) return c;
    c = int_vec_compare(a->residual_structural_support, b->residual_structural_support, n);
    if (c != 0) return c;
    int min_len = a->route_sequence_length < b->route_sequence_length
        ? a->route_sequence_length : b->route_sequence_length;
    c = int_vec_compare(a->route_sequence, b->route_sequence, min_len);
    if (c != 0) return c;
    if (a->route_sequence_length < b->route_sequence_length) return -1;
    if (a->route_sequence_length > b->route_sequence_length) return  1;
    return 0;
}

static const p2_ctx_t* g_p2_qsort_ctx = NULL;
static int p2_qsort_state_cmp(const void* lhs, const void* rhs) {
    return p2_state_sort_compare(
        g_p2_qsort_ctx, *(const p2_state_t* const*)lhs, *(const p2_state_t* const*)rhs);
}
static void bucket_sort(const p2_ctx_t* ctx, p2_bucket_t* bucket) {
    if (bucket->count <= 1) return;
    g_p2_qsort_ctx = ctx;
    qsort(bucket->items, (size_t)bucket->count, sizeof(p2_state_t*), p2_qsort_state_cmp);
    g_p2_qsort_ctx = NULL;
}

static int labels_equal(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b
) {
    if (a->current_node != b->current_node) return 0;
    if (a->cost != b->cost) return 0;
    if (vec_compare(a->resources, b->resources, ctx->global_dim) != 0) return 0;
    if (int_vec_compare(a->unreachable_vector, b->unreachable_vector,
                        ctx->num_required_customers) != 0) return 0;
    return 1;
}

static int label_dominates(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b
) {
    if (a->current_node != b->current_node) return 0;
    if (!vec_le(a->resources, b->resources, ctx->global_dim)) return 0;
    if (a->cost > b->cost) return 0;
    int n = ctx->num_required_customers;
    /* phase2_customer_state_no_worse */
    for (int i = 0; i < n; ++i) {
        int ar = p2_customer_state_rank(a->unreachable_vector[i]);
        int br = p2_customer_state_rank(b->unreachable_vector[i]);
        if (ar > br) return 0;
    }
    int strict =
        (a->cost < b->cost)
        || vec_lt(a->resources, b->resources, ctx->global_dim);
    if (!strict) {
        for (int i = 0; i < n; ++i) {
            int ar = p2_customer_state_rank(a->unreachable_vector[i]);
            int br = p2_customer_state_rank(b->unreachable_vector[i]);
            if (ar < br) { strict = 1; break; }
        }
    }
    return strict;
}

static int state_dominates(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b
) {
    int dominates_label = label_dominates(ctx, a, b);
    int eq = labels_equal(ctx, a, b);
    if (!dominates_label && !eq) return 0;

    int n = ctx->num_required_customers;
    int vectors_match = (int_vec_compare(a->unreachable_vector, b->unreachable_vector, n) == 0);
    if (vectors_match) {
        if (!sig_no_worse(a->sequence_structural_profile, b->sequence_structural_profile, n))
            return 0;
        if (!sig_no_worse(a->residual_structural_support, b->residual_structural_support, n))
            return 0;
        if (sig_strictly_better(a->sequence_structural_profile,
                                b->sequence_structural_profile, n)
            || sig_strictly_better(a->residual_structural_support,
                                   b->residual_structural_support, n)) {
            return 1;
        }
    }
    /* labels equal AND a's route_sequence < b's route_sequence */
    if (eq) {
        int min_len = a->route_sequence_length < b->route_sequence_length
            ? a->route_sequence_length : b->route_sequence_length;
        int c = int_vec_compare(a->route_sequence, b->route_sequence, min_len);
        if (c < 0) return 1;
        if (c == 0 && a->route_sequence_length < b->route_sequence_length) return 1;
    }
    return 0;
}

static int states_equivalent(
    const p2_ctx_t* ctx, const p2_state_t* a, const p2_state_t* b
) {
    if (!labels_equal(ctx, a, b)) return 0;
    int n = ctx->num_required_customers;
    if (a->route_sequence_length != b->route_sequence_length) return 0;
    if (int_vec_compare(a->route_sequence, b->route_sequence, a->route_sequence_length) != 0)
        return 0;
    if (int_vec_compare(a->sequence_structural_profile, b->sequence_structural_profile, n) != 0)
        return 0;
    if (int_vec_compare(a->residual_structural_support, b->residual_structural_support, n) != 0)
        return 0;
    return 1;
}

static int insert_with_dominance(
    p2_ctx_t* ctx, p2_bucket_t* bucket, p2_state_t* candidate
) {
    for (int i = 0; i < bucket->count; ++i) {
        const p2_state_t* old = bucket->items[i];
        if (states_equivalent(ctx, old, candidate)) return 0;
        if (state_dominates(ctx, old, candidate))   return 0;
    }
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
    return 1;
}

/* ---------- Bucket and queue helpers ---------- */

static int bucket_grow(p2_ctx_t* ctx, p2_bucket_t* bucket, int min_capacity) {
    if (bucket->capacity >= min_capacity) return 0;
    int new_cap = bucket->capacity > 0 ? bucket->capacity : 4;
    while (new_cap < min_capacity) new_cap *= 2;
    p2_state_t** items = mespprc_arena_alloc(
        ctx->arena, sizeof(p2_state_t*) * (size_t)new_cap, sizeof(void*));
    if (!items) return -1;
    if (bucket->count > 0) {
        memcpy(items, bucket->items, sizeof(p2_state_t*) * (size_t)bucket->count);
    }
    bucket->items = items;
    bucket->capacity = new_cap;
    return 0;
}
static int bucket_append(p2_ctx_t* ctx, p2_bucket_t* bucket, p2_state_t* state) {
    if (bucket_grow(ctx, bucket, bucket->count + 1) != 0) return -1;
    bucket->items[bucket->count++] = state;
    return 0;
}
static int bucket_remove_at(p2_bucket_t* bucket, int index) {
    if (index < 0 || index >= bucket->count) return -1;
    for (int i = index + 1; i < bucket->count; ++i) {
        bucket->items[i - 1] = bucket->items[i];
    }
    bucket->count--;
    return 0;
}

static void active_push(p2_ctx_t* ctx, int node) {
    if (ctx->in_active_queue[node]) return;
    ctx->active_queue[ctx->active_count++] = node;
    ctx->in_active_queue[node] = 1;
}
static int active_pop(p2_ctx_t* ctx) {
    int node = ctx->active_queue[0];
    for (int i = 1; i < ctx->active_count; ++i) {
        ctx->active_queue[i - 1] = ctx->active_queue[i];
    }
    ctx->active_count--;
    ctx->in_active_queue[node] = 0;
    return node;
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
static int within_limits(const double* v, const double* limit, int n) {
    for (int i = 0; i < n; ++i) if (v[i] > limit[i] + 1e-12) return 0;
    return 1;
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
static int sig_no_worse(const int* a, const int* b, int n) {
    for (int i = 0; i < n; ++i) if (a[i] > b[i]) return 0;
    return 1;
}
static int sig_strictly_better(const int* a, const int* b, int n) {
    for (int i = 0; i < n; ++i) if (a[i] < b[i]) return 1;
    return 0;
}
