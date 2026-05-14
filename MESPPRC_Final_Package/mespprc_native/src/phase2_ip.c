/*
 * Phase 2 IP (Phase D) — set-partitioning MIP via HiGHS C API.
 *
 * Mirrors mespprc/phase2_ip.py exactly:
 *
 *   1. Extract per-route metadata from the Phase 1 result: cost, global
 *      resources, required-covered customer set.
 *   2. Reduce the route pool with the same IP-side dominance predicate as
 *      `Phase2IPSolver._reduce_route_pool`: drop routes that cover no
 *      required customers; drop duplicates under
 *      (required_covered, cost, tuple(global_resources)); pairwise dominance
 *      within identical required-covered groups (cost <= and global_res <=
 *      with strict inequality somewhere).
 *   3. Build the MIP:
 *        min  sum_i  cost_i * x_i
 *        s.t. sum_{i covers c}  x_i == 1   for each required customer c
 *             sum_i  global_res[d][i] * x_i  <= global_limit[d]   for each d
 *             x_i ∈ {0,1}
 *   4. Solve the relaxed model first (no global-resource rows). If HiGHS
 *      reports infeasible → ROUTE_SET_INFEASIBLE.
 *   5. Solve the constrained model. If HiGHS reports infeasible →
 *      GLOBAL_LIMITS_INFEASIBLE. Otherwise take the optimal selection.
 *
 * The HiGHS handle is used through its C API and is created/destroyed inside
 * each solve. All scratch and result memory lives in mespprc arenas.
 */

#include "internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(MESPPRC_HAVE_HIGHS) && MESPPRC_HAVE_HIGHS
#include "highs_c_api.h"
#endif

/* ---------- Result struct ---------- */

struct mespprc_phase2_ip_result {
    int     status;
    int     infeasibility_reason;
    int     is_feasible;
    int     coverage_complete;
    double  total_cost;

    int     selected_route_count;
    int*    selected_phase1_indices;     /* 0-based into Phase 1 result */

    int     original_route_count;
    int     reduced_route_count;

    mespprc_arena_t* arena;
};

/* ---------- Internal route metadata (slimmer than Phase 2 DP's) ---------- */

typedef struct {
    int     phase1_index;            /* 0-based into Phase 1 result */
    int     route_id;                /* 1-based, mirrors Python convention */
    double  cost;
    double* global_resources;        /* global_dim entries; NULL if global_dim == 0 */
    int*    required_covered;        /* sorted dense customer indices */
    int     required_covered_count;
} ip_route_t;

typedef struct {
    const mespprc_instance_t* instance;
    const mespprc_phase1_result_t* phase1;

    int     num_required_customers;
    int*    required_customer_ids;       /* external node ids in ascending id order */
    int*    required_idx_for_external;   /* max_external_id + 1 entries; -1 if not required */
    int     max_external_required_id;

    int     global_dim;

    ip_route_t* routes;                  /* original count, then compacted to kept */
    int         route_count;             /* original count */
    int*        keep;                    /* route_count entries; 1 if not removed */
    int         kept_count;
    int*        kept_indices;            /* kept_count entries; index into routes */

    mespprc_arena_t* arena;
} ip_ctx_t;

/* ---------- Forward declarations ---------- */

static int  ip_prepare_routes(ip_ctx_t* ctx);
static int  ip_reduce_route_pool(ip_ctx_t* ctx);
static int  ip_solve_with_highs(
    ip_ctx_t* ctx, int include_global_limits,
    int* out_status,                        /* 0=optimal, 1=infeasible, -1=other */
    double* out_objective,                  /* valid iff *out_status == 0 */
    int* out_selected_kept_count,
    int** out_selected_kept_indices         /* arena-allocated indices into kept_indices */
);

static int  int_cmp_qsort(const void* a, const void* b);
static int  vec_le(const double* a, const double* b, int n);
static int  vec_lt(const double* a, const double* b, int n);

/* ---------- Public entry point ---------- */

mespprc_status_t mespprc_solve_phase2_ip(
    const mespprc_instance_t* instance,
    const mespprc_phase1_result_t* routes,
    mespprc_phase2_ip_result_t** out_result
) {
    if (!instance || !routes || !out_result) return MESPPRC_ERR_INVALID_ARG;
    if (!instance->finalized) return MESPPRC_ERR_NOT_FINALIZED;
    *out_result = NULL;

#if !(defined(MESPPRC_HAVE_HIGHS) && MESPPRC_HAVE_HIGHS)
    return MESPPRC_ERR_NOT_IMPLEMENTED;
#else

    mespprc_arena_t* scratch = mespprc_arena_create(0);
    if (!scratch) return MESPPRC_ERR_NOMEM;

    ip_ctx_t* ctx = mespprc_arena_calloc(scratch, sizeof(ip_ctx_t), 0);
    if (!ctx) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
    ctx->instance = instance;
    ctx->phase1 = routes;
    ctx->global_dim = instance->global_dim;
    ctx->arena = scratch;

    /* Build required-customer index in ascending id order — matches
     * mespprc.MESPPRCInstance.customers(). */
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
    ctx->required_customer_ids = mespprc_arena_alloc(
        scratch, sizeof(int) * (size_t)(n_cust > 0 ? n_cust : 1), sizeof(int));
    ctx->required_idx_for_external = mespprc_arena_alloc(
        scratch, sizeof(int) * (size_t)(max_id + 1), sizeof(int));
    if (!ctx->required_customer_ids || !ctx->required_idx_for_external) {
        mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM;
    }
    for (int i = 0; i <= max_id; ++i) ctx->required_idx_for_external[i] = -1;
    {
        int* ids = mespprc_arena_alloc(
            scratch, sizeof(int) * (size_t)(n_cust > 0 ? n_cust : 1), sizeof(int));
        if (!ids) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
        int k = 0;
        for (int v = 0; v < instance->num_nodes; ++v) {
            if (instance->nodes[v].type == MESPPRC_NODE_TYPE_CUSTOMER) {
                ids[k++] = instance->nodes[v].id;
            }
        }
        qsort(ids, (size_t)n_cust, sizeof(int), int_cmp_qsort);
        for (int i = 0; i < n_cust; ++i) {
            ctx->required_customer_ids[i] = ids[i];
            ctx->required_idx_for_external[ids[i]] = i;
        }
    }

    /* Step 1: extract per-route metadata. */
    if (ip_prepare_routes(ctx) != 0) {
        mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM;
    }

    /* Step 2: route pool reduction. */
    if (ip_reduce_route_pool(ctx) != 0) {
        mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM;
    }

    /* Allocate the result up front so the early-exit paths can populate it. */
    mespprc_arena_t* result_arena = mespprc_arena_create(0);
    if (!result_arena) { mespprc_arena_destroy(scratch); return MESPPRC_ERR_NOMEM; }
    mespprc_phase2_ip_result_t* result =
        mespprc_arena_calloc(result_arena, sizeof(*result), 0);
    if (!result) {
        mespprc_arena_destroy(result_arena);
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    result->arena = result_arena;
    result->total_cost = NAN;
    result->original_route_count = ctx->route_count;
    result->reduced_route_count = ctx->kept_count;

    /* Quick coverage check — if any required customer has no supporting
     * reduced route, there is no way to cover it and the IP is infeasible
     * with reason ROUTE_SET. Mirrors the Python `uncovered_customers` shortcut. */
    {
        int* covered = mespprc_arena_calloc(
            scratch,
            sizeof(int) * (size_t)(ctx->num_required_customers > 0 ? ctx->num_required_customers : 1),
            sizeof(int));
        if (!covered) {
            mespprc_arena_destroy(result_arena);
            mespprc_arena_destroy(scratch);
            return MESPPRC_ERR_NOMEM;
        }
        for (int k = 0; k < ctx->kept_count; ++k) {
            const ip_route_t* r = &ctx->routes[ctx->kept_indices[k]];
            for (int t = 0; t < r->required_covered_count; ++t) {
                covered[r->required_covered[t]] = 1;
            }
        }
        for (int i = 0; i < ctx->num_required_customers; ++i) {
            if (!covered[i]) {
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

    /* Step 3: relaxed solve (no global rows). Detects ROUTE_SET infeasibility. */
    int relaxed_status = -1;
    double relaxed_obj = 0.0;
    int relaxed_sel_count = 0;
    int* relaxed_sel = NULL;
    if (ip_solve_with_highs(ctx, /*include_global_limits=*/0,
                            &relaxed_status, &relaxed_obj,
                            &relaxed_sel_count, &relaxed_sel) != 0) {
        mespprc_arena_destroy(result_arena);
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    if (relaxed_status == 1) {
        result->status = MESPPRC_PHASE2_STATUS_INFEASIBLE;
        result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_ROUTE_SET_INFEASIBLE;
        result->is_feasible = 0;
        result->coverage_complete = 0;
        mespprc_arena_destroy(scratch);
        *out_result = result;
        return MESPPRC_OK;
    }
    if (relaxed_status != 0) {
        mespprc_arena_destroy(result_arena);
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_INVALID_ARG;
    }

    /* Step 4: constrained solve. Detects GLOBAL_LIMITS infeasibility. */
    int con_status = -1;
    double con_obj = 0.0;
    int con_sel_count = 0;
    int* con_sel = NULL;
    if (ip_solve_with_highs(ctx, /*include_global_limits=*/1,
                            &con_status, &con_obj,
                            &con_sel_count, &con_sel) != 0) {
        mespprc_arena_destroy(result_arena);
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_NOMEM;
    }
    if (con_status == 1) {
        result->status = MESPPRC_PHASE2_STATUS_INFEASIBLE;
        result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_GLOBAL_LIMITS;
        result->is_feasible = 0;
        result->coverage_complete = 0;
        mespprc_arena_destroy(scratch);
        *out_result = result;
        return MESPPRC_OK;
    }
    if (con_status != 0) {
        mespprc_arena_destroy(result_arena);
        mespprc_arena_destroy(scratch);
        return MESPPRC_ERR_INVALID_ARG;
    }

    /* Optimal found. Translate column indices back to phase1 indices. */
    result->status = MESPPRC_PHASE2_STATUS_OPTIMAL;
    result->infeasibility_reason = MESPPRC_PHASE2_INFEAS_NONE;
    result->is_feasible = 1;
    result->coverage_complete = 1;
    result->total_cost = con_obj;
    result->selected_route_count = con_sel_count;
    if (con_sel_count > 0) {
        result->selected_phase1_indices = mespprc_arena_alloc(
            result_arena, sizeof(int) * (size_t)con_sel_count, sizeof(int));
        if (!result->selected_phase1_indices) {
            mespprc_arena_destroy(result_arena);
            mespprc_arena_destroy(scratch);
            return MESPPRC_ERR_NOMEM;
        }
        for (int i = 0; i < con_sel_count; ++i) {
            int kept_idx = con_sel[i];                       /* index into kept_indices */
            int route_idx = ctx->kept_indices[kept_idx];     /* index into routes */
            result->selected_phase1_indices[i] =
                ctx->routes[route_idx].phase1_index;
        }
    }

    mespprc_arena_destroy(scratch);
    *out_result = result;
    return MESPPRC_OK;
#endif
}

void mespprc_phase2_ip_result_destroy(mespprc_phase2_ip_result_t* result) {
    if (!result) return;
    mespprc_arena_t* arena = result->arena;
    mespprc_arena_destroy(arena);
}

/* ---------- Result accessors ---------- */

int mespprc_phase2_ip_status(const mespprc_phase2_ip_result_t* r) {
    return r ? r->status : -1;
}
int mespprc_phase2_ip_infeasibility_reason(const mespprc_phase2_ip_result_t* r) {
    return r ? r->infeasibility_reason : -1;
}
int mespprc_phase2_ip_is_feasible(const mespprc_phase2_ip_result_t* r) {
    return r ? r->is_feasible : 0;
}
int mespprc_phase2_ip_coverage_complete(const mespprc_phase2_ip_result_t* r) {
    return r ? r->coverage_complete : 0;
}
mespprc_status_t mespprc_phase2_ip_total_cost(
    const mespprc_phase2_ip_result_t* r, double* out_cost
) {
    if (!r || !out_cost) return MESPPRC_ERR_INVALID_ARG;
    *out_cost = r->total_cost;
    return MESPPRC_OK;
}
int mespprc_phase2_ip_selected_route_count(const mespprc_phase2_ip_result_t* r) {
    return r ? r->selected_route_count : 0;
}
mespprc_status_t mespprc_phase2_ip_selected_routes(
    const mespprc_phase2_ip_result_t* r, int* out, int cap
) {
    if (!r || !out) return MESPPRC_ERR_INVALID_ARG;
    if (cap < r->selected_route_count) return MESPPRC_ERR_BUFFER_TOO_SMALL;
    for (int i = 0; i < r->selected_route_count; ++i) {
        out[i] = r->selected_phase1_indices[i];
    }
    return MESPPRC_OK;
}
int mespprc_phase2_ip_original_route_count(const mespprc_phase2_ip_result_t* r) {
    return r ? r->original_route_count : 0;
}
int mespprc_phase2_ip_reduced_route_count(const mespprc_phase2_ip_result_t* r) {
    return r ? r->reduced_route_count : 0;
}

/* ---------- Implementation helpers ---------- */

static int int_cmp_qsort(const void* a, const void* b) {
    int ai = *(const int*)a, bi = *(const int*)b;
    return (ai > bi) - (ai < bi);
}

static int vec_le(const double* a, const double* b, int n) {
    for (int i = 0; i < n; ++i) if (a[i] > b[i]) return 0;
    return 1;
}

static int vec_lt(const double* a, const double* b, int n) {
    int saw_lt = 0;
    for (int i = 0; i < n; ++i) {
        if (a[i] > b[i]) return 0;
        if (a[i] < b[i]) saw_lt = 1;
    }
    return saw_lt;
}

static int ip_prepare_routes(ip_ctx_t* ctx) {
    int n = ctx->phase1->route_count;
    int n_all = ctx->phase1->num_customers;
    int n_required = ctx->num_required_customers;

    ctx->routes = n > 0
        ? mespprc_arena_calloc(ctx->arena, sizeof(ip_route_t) * (size_t)n, sizeof(ip_route_t))
        : NULL;
    if (n > 0 && !ctx->routes) return -1;
    ctx->route_count = n;

    for (int i = 0; i < n; ++i) {
        ip_route_t* r = &ctx->routes[i];
        r->phase1_index = i;
        r->route_id = i + 1;
        r->cost = ctx->phase1->costs[i];

        if (ctx->global_dim > 0) {
            r->global_resources = mespprc_arena_alloc(
                ctx->arena, sizeof(double) * (size_t)ctx->global_dim, sizeof(double));
            if (!r->global_resources) return -1;
            memcpy(r->global_resources,
                   ctx->phase1->global_resources + (size_t)i * ctx->global_dim,
                   (size_t)ctx->global_dim * sizeof(double));
        }

        /* required_covered: dense indices c where the route's customer-state
         * signature reports that customer c was visited (signature value > 0). */
        int* req_buf = mespprc_arena_alloc(
            ctx->arena, sizeof(int) * (size_t)(n_required > 0 ? n_required : 1),
            sizeof(int));
        if (!req_buf) return -1;
        int req_count = 0;
        const int* sig = ctx->phase1->customer_state_sigs + (size_t)i * (n_all > 0 ? n_all : 0);
        for (int c = 0; c < n_required; ++c) {
            int sig_value = (c < n_all) ? sig[c] : 0;
            if (sig_value > 0) req_buf[req_count++] = c;
        }
        r->required_covered = req_buf;
        r->required_covered_count = req_count;
    }
    return 0;
}

/* Same predicate as Phase2IPSolver._route_dominates_for_ip:
 *   same required_covered set, a.cost <= b.cost, a's global resources
 *   componentwise <= b's, with strict inequality somewhere. */
static int ip_route_dominates(const ip_ctx_t* ctx, const ip_route_t* a, const ip_route_t* b) {
    if (a->required_covered_count != b->required_covered_count) return 0;
    for (int k = 0; k < a->required_covered_count; ++k) {
        if (a->required_covered[k] != b->required_covered[k]) return 0;
    }
    if (a->cost > b->cost) return 0;
    if (ctx->global_dim > 0) {
        if (!vec_le(a->global_resources, b->global_resources, ctx->global_dim)) return 0;
    }
    int strict =
        (a->cost < b->cost)
        || (ctx->global_dim > 0
            && vec_lt(a->global_resources, b->global_resources, ctx->global_dim));
    return strict;
}

/* Lexicographic compare for the reduction sort key — matches Python's
 * `_route_reduction_key`: (sorted required_covered, cost, global_resources, route_id). */
static int ip_reduction_key_cmp(const ip_ctx_t* ctx, const ip_route_t* a, const ip_route_t* b) {
    int min_n = a->required_covered_count < b->required_covered_count
        ? a->required_covered_count : b->required_covered_count;
    for (int k = 0; k < min_n; ++k) {
        int ai = ctx->required_customer_ids[a->required_covered[k]];
        int bi = ctx->required_customer_ids[b->required_covered[k]];
        if (ai != bi) return (ai < bi) ? -1 : 1;
    }
    if (a->required_covered_count != b->required_covered_count) {
        return (a->required_covered_count < b->required_covered_count) ? -1 : 1;
    }
    if (a->cost != b->cost) return (a->cost < b->cost) ? -1 : 1;
    for (int d = 0; d < ctx->global_dim; ++d) {
        if (a->global_resources[d] != b->global_resources[d]) {
            return (a->global_resources[d] < b->global_resources[d]) ? -1 : 1;
        }
    }
    if (a->route_id != b->route_id) return (a->route_id < b->route_id) ? -1 : 1;
    return 0;
}

static int ip_reduce_route_pool(ip_ctx_t* ctx) {
    int n = ctx->route_count;
    ctx->keep = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)(n > 0 ? n : 1), sizeof(int));
    if (n > 0 && !ctx->keep) return -1;
    for (int i = 0; i < n; ++i) ctx->keep[i] = 1;

    /* (a) Drop routes that cover no required customers AND have non-negative
     * cost AND non-negative global resources — matches Python's first filter. */
    for (int i = 0; i < n; ++i) {
        const ip_route_t* r = &ctx->routes[i];
        if (r->required_covered_count > 0) continue;
        if (r->cost < 0) continue;
        int neg_g = 0;
        for (int d = 0; d < ctx->global_dim; ++d) {
            if (r->global_resources[d] < 0) { neg_g = 1; break; }
        }
        if (neg_g) continue;
        ctx->keep[i] = 0;
    }

    /* (b) Drop duplicates by (required_covered, cost, global_resources). The
     * Python code keeps the first occurrence; we do the same by O(n^2)
     * comparison (route counts are typically small after Phase 1). */
    for (int i = 0; i < n; ++i) {
        if (!ctx->keep[i]) continue;
        const ip_route_t* a = &ctx->routes[i];
        for (int j = i + 1; j < n; ++j) {
            if (!ctx->keep[j]) continue;
            const ip_route_t* b = &ctx->routes[j];
            if (a->required_covered_count != b->required_covered_count) continue;
            int eq = 1;
            for (int k = 0; k < a->required_covered_count; ++k) {
                if (a->required_covered[k] != b->required_covered[k]) { eq = 0; break; }
            }
            if (!eq) continue;
            if (a->cost != b->cost) continue;
            int g_eq = 1;
            for (int d = 0; d < ctx->global_dim; ++d) {
                if (a->global_resources[d] != b->global_resources[d]) { g_eq = 0; break; }
            }
            if (!g_eq) continue;
            ctx->keep[j] = 0;          /* keep first occurrence */
        }
    }

    /* (c) Pairwise dominance within identical required_covered groups. The
     * Python implementation processes routes in `_route_reduction_key` order
     * to make the result deterministic. We do the same. */
    int* order = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)(n > 0 ? n : 1), sizeof(int));
    if (n > 0 && !order) return -1;
    int order_count = 0;
    for (int i = 0; i < n; ++i) if (ctx->keep[i]) order[order_count++] = i;
    /* Insertion sort by reduction key. */
    for (int i = 1; i < order_count; ++i) {
        int cur = order[i];
        int j = i - 1;
        while (j >= 0 && ip_reduction_key_cmp(ctx, &ctx->routes[order[j]], &ctx->routes[cur]) > 0) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = cur;
    }

    for (int gi = 0; gi < order_count; ++gi) {
        int i = order[gi];
        if (!ctx->keep[i]) continue;
        const ip_route_t* a = &ctx->routes[i];
        for (int gj = gi + 1; gj < order_count; ++gj) {
            int j = order[gj];
            if (!ctx->keep[j]) continue;
            const ip_route_t* b = &ctx->routes[j];
            /* Different required_covered → no dominance possible. */
            if (a->required_covered_count != b->required_covered_count) continue;
            int eq = 1;
            for (int k = 0; k < a->required_covered_count; ++k) {
                if (a->required_covered[k] != b->required_covered[k]) { eq = 0; break; }
            }
            if (!eq) continue;
            if (ip_route_dominates(ctx, a, b)) {
                ctx->keep[j] = 0;
            } else if (ip_route_dominates(ctx, b, a)) {
                ctx->keep[i] = 0;
                break;     /* a is gone; move on */
            }
        }
    }

    /* (d) Compact survivors in reduction-key order. */
    int kept = 0;
    for (int i = 0; i < order_count; ++i) if (ctx->keep[order[i]]) kept++;
    ctx->kept_indices = mespprc_arena_alloc(
        ctx->arena, sizeof(int) * (size_t)(kept > 0 ? kept : 1), sizeof(int));
    if (kept > 0 && !ctx->kept_indices) return -1;
    int p = 0;
    for (int i = 0; i < order_count; ++i) {
        if (ctx->keep[order[i]]) ctx->kept_indices[p++] = order[i];
    }
    ctx->kept_count = kept;
    return 0;
}

#if defined(MESPPRC_HAVE_HIGHS) && MESPPRC_HAVE_HIGHS

/* Build and solve the MIP with HiGHS. The model is column-major:
 *   columns: one per kept route
 *   rows:    num_required (equality, == 1) [+ global_dim (<=, <= limit)]
 * The number of nonzeros per column is precisely
 *   required_covered_count + (include_global_limits ? global_dim : 0). */
static int ip_solve_with_highs(
    ip_ctx_t* ctx,
    int include_global_limits,
    int* out_status,
    double* out_objective,
    int* out_selected_kept_count,
    int** out_selected_kept_indices
) {
    *out_status = -1;
    *out_objective = 0.0;
    *out_selected_kept_count = 0;
    *out_selected_kept_indices = NULL;

    int num_col = ctx->kept_count;
    int num_required = ctx->num_required_customers;
    int extra_rows = include_global_limits ? ctx->global_dim : 0;
    int num_row = num_required + extra_rows;

    /* Edge case: empty model (no required customers and no extra rows). HiGHS
     * still accepts num_row=0, but for a partitioning problem with required
     * customers the row count is always num_required. */

    /* Count nonzeros and build column-major arrays. */
    HighsInt num_nz = 0;
    for (int k = 0; k < num_col; ++k) {
        const ip_route_t* r = &ctx->routes[ctx->kept_indices[k]];
        num_nz += r->required_covered_count;
        if (extra_rows > 0) {
            for (int d = 0; d < ctx->global_dim; ++d) {
                if (r->global_resources[d] != 0.0) num_nz++;
            }
        }
    }

    /* Allocate column-major CSC arrays + bounds + integrality. */
    double* col_cost  = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_col > 0 ? num_col : 1), sizeof(double));
    double* col_lower = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_col > 0 ? num_col : 1), sizeof(double));
    double* col_upper = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_col > 0 ? num_col : 1), sizeof(double));
    HighsInt* integrality = mespprc_arena_alloc(ctx->arena,
        sizeof(HighsInt) * (size_t)(num_col > 0 ? num_col : 1), sizeof(HighsInt));
    double* row_lower = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_row > 0 ? num_row : 1), sizeof(double));
    double* row_upper = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_row > 0 ? num_row : 1), sizeof(double));
    HighsInt* a_start = mespprc_arena_alloc(ctx->arena,
        sizeof(HighsInt) * (size_t)(num_col + 1), sizeof(HighsInt));
    HighsInt* a_index = mespprc_arena_alloc(ctx->arena,
        sizeof(HighsInt) * (size_t)(num_nz > 0 ? num_nz : 1), sizeof(HighsInt));
    double* a_value = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_nz > 0 ? num_nz : 1), sizeof(double));
    if (!col_cost || !col_lower || !col_upper || !integrality
        || !row_lower || !row_upper || !a_start || !a_index || !a_value) return -1;

    for (int k = 0; k < num_col; ++k) {
        const ip_route_t* r = &ctx->routes[ctx->kept_indices[k]];
        col_cost[k] = r->cost;
        col_lower[k] = 0.0;
        col_upper[k] = 1.0;
        integrality[k] = kHighsVarTypeInteger;
    }
    for (int c = 0; c < num_required; ++c) {
        row_lower[c] = 1.0;
        row_upper[c] = 1.0;
    }
    if (extra_rows > 0) {
        /* HiGHS uses ±kHighsInfinity for unbounded. We need <= limit, so set
         * lower to -inf. Use a very-negative sentinel; HiGHS_run will treat
         * it as -∞ via Highs_getInfinity() if we substitute later. We use
         * -1e30 which HiGHS interprets as -inf by default. */
        for (int d = 0; d < ctx->global_dim; ++d) {
            row_lower[num_required + d] = -1e30;
            row_upper[num_required + d] = ctx->instance->global_limits[d];
        }
    }

    HighsInt nz = 0;
    for (int k = 0; k < num_col; ++k) {
        a_start[k] = nz;
        const ip_route_t* r = &ctx->routes[ctx->kept_indices[k]];
        /* Coverage rows: a_index = required dense customer index, value 1. */
        for (int t = 0; t < r->required_covered_count; ++t) {
            a_index[nz] = r->required_covered[t];
            a_value[nz] = 1.0;
            nz++;
        }
        /* Global rows: dense index = num_required + d. */
        if (extra_rows > 0) {
            for (int d = 0; d < ctx->global_dim; ++d) {
                if (r->global_resources[d] != 0.0) {
                    a_index[nz] = num_required + d;
                    a_value[nz] = r->global_resources[d];
                    nz++;
                }
            }
        }
    }
    a_start[num_col] = nz;

    void* h = Highs_create();
    if (!h) return -1;

    /* Quiet output, modest threading. */
    Highs_setBoolOptionValue(h, "output_flag", 0);
    Highs_setStringOptionValue(h, "presolve", "on");
    Highs_setIntOptionValue(h, "threads", 1);

    HighsInt rc = Highs_passMip(
        h,
        (HighsInt)num_col, (HighsInt)num_row, (HighsInt)num_nz,
        kHighsMatrixFormatColwise, kHighsObjSenseMinimize,
        /*offset=*/0.0,
        col_cost, col_lower, col_upper,
        row_lower, row_upper,
        a_start, a_index, a_value,
        integrality
    );
    if (rc != 0) {
        Highs_destroy(h);
        return -1;
    }

    rc = Highs_run(h);
    if (rc != 0) {
        Highs_destroy(h);
        return -1;
    }

    HighsInt model_status = Highs_getModelStatus(h);
    if (model_status == kHighsModelStatusInfeasible) {
        *out_status = 1;
        Highs_destroy(h);
        return 0;
    }
    if (model_status != kHighsModelStatusOptimal) {
        Highs_destroy(h);
        return -1;
    }

    *out_objective = Highs_getObjectiveValue(h);

    double* col_value = mespprc_arena_alloc(ctx->arena,
        sizeof(double) * (size_t)(num_col > 0 ? num_col : 1), sizeof(double));
    if (!col_value) {
        Highs_destroy(h);
        return -1;
    }
    rc = Highs_getSolution(h, col_value, NULL, NULL, NULL);
    if (rc != 0) {
        Highs_destroy(h);
        return -1;
    }

    int sel = 0;
    for (int k = 0; k < num_col; ++k) {
        if (col_value[k] > 0.5) sel++;
    }
    int* sel_buf = mespprc_arena_alloc(ctx->arena,
        sizeof(int) * (size_t)(sel > 0 ? sel : 1), sizeof(int));
    if (!sel_buf) {
        Highs_destroy(h);
        return -1;
    }
    int p = 0;
    for (int k = 0; k < num_col; ++k) {
        if (col_value[k] > 0.5) sel_buf[p++] = k;
    }

    *out_status = 0;
    *out_selected_kept_count = sel;
    *out_selected_kept_indices = sel_buf;

    Highs_destroy(h);
    return 0;
}

#else  /* MESPPRC_HAVE_HIGHS */

static int ip_solve_with_highs(
    ip_ctx_t* ctx,
    int include_global_limits,
    int* out_status,
    double* out_objective,
    int* out_selected_kept_count,
    int** out_selected_kept_indices
) {
    (void)ctx; (void)include_global_limits;
    *out_status = -1; *out_objective = 0.0;
    *out_selected_kept_count = 0; *out_selected_kept_indices = NULL;
    return -1;
}

#endif
