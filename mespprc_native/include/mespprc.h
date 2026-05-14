#ifndef MESPPRC_H
#define MESPPRC_H

/*
 * mespprc_native public C ABI.
 *
 * The Python binding in mespprc_native/python/_native.py is the canonical
 * consumer of this header and mirrors every visible struct via ctypes.
 * mespprc_struct_sizes() is the layout self-check the binding asserts at
 * import time so size or alignment drift between the two languages is
 * caught before any solver is invoked.
 *
 * Memory ownership: every handle returned by an entry point is owned by the
 * library and must be released through its matching `*_destroy` function.
 * The library does not retain any pointers into caller-owned memory after a
 * call returns.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  #ifdef MESPPRC_BUILDING_DLL
    #define MESPPRC_API __declspec(dllexport)
  #else
    #define MESPPRC_API __declspec(dllimport)
  #endif
#else
  #define MESPPRC_API __attribute__((visibility("default")))
#endif

/* ---------- Status codes ---------- */

typedef enum {
    MESPPRC_OK                  = 0,
    MESPPRC_ERR_NOMEM           = 1,
    MESPPRC_ERR_INVALID_ARG     = 2,
    MESPPRC_ERR_NOT_IMPLEMENTED = 3,
    MESPPRC_ERR_INSTANCE_INVALID = 4,
    MESPPRC_ERR_BUFFER_TOO_SMALL = 5,
    MESPPRC_ERR_DUPLICATE        = 6,
    MESPPRC_ERR_NOT_FINALIZED    = 7
} mespprc_status_t;

/* ---------- Node types ---------- */

#define MESPPRC_NODE_TYPE_SOURCE   0
#define MESPPRC_NODE_TYPE_CUSTOMER 1
#define MESPPRC_NODE_TYPE_SINK     2

/* ---------- Opaque handles ---------- */

typedef struct mespprc_instance mespprc_instance_t;
typedef struct mespprc_phase1_result mespprc_phase1_result_t;
typedef struct mespprc_phase2_dp_result mespprc_phase2_dp_result_t;
typedef struct mespprc_phase2_ip_result mespprc_phase2_ip_result_t;

/* ---------- Layout self-check ---------- *
 *
 * The Python binding mirrors every struct that it dereferences. To catch
 * accidental ABI drift early, mespprc_struct_sizes() reports sizeof() for
 * every such struct. The binding asserts these match what its own
 * ctypes.Structure subclasses report at import time.
 */

typedef struct mespprc_struct_sizes {
    uint64_t struct_sizes;
    uint64_t status_t;
    uint64_t pointer;
    uint64_t double_;
    uint64_t int_;
} mespprc_struct_sizes_t;

MESPPRC_API void mespprc_struct_sizes(mespprc_struct_sizes_t* out);

/* ---------- Library version ---------- */

MESPPRC_API const char* mespprc_version(void);

/* ---------- Instance construction ---------- *
 *
 * Typical lifecycle:
 *
 *   mespprc_instance_t* inst = NULL;
 *   mespprc_instance_create(num_customers, local_dim, global_dim,
 *                           expected_arc_count, &inst);
 *   mespprc_instance_set_local_limits(inst, local_limits, local_dim);
 *   mespprc_instance_set_global_limits(inst, global_limits, global_dim);
 *   mespprc_instance_add_node(inst, source_id, MESPPRC_NODE_TYPE_SOURCE);
 *   for (...) mespprc_instance_add_node(inst, c, MESPPRC_NODE_TYPE_CUSTOMER);
 *   mespprc_instance_add_node(inst, sink_id, MESPPRC_NODE_TYPE_SINK);
 *   for (...) mespprc_instance_add_arc(inst, t, h, cost,
 *                                      local_res, global_res);
 *   mespprc_instance_finalize(inst);
 *   ...
 *   mespprc_instance_destroy(inst);
 *
 * `expected_arc_count` is a capacity hint. The instance will grow if more arcs
 * are added.
 */

MESPPRC_API mespprc_status_t mespprc_instance_create(
    int num_nodes,
    int local_dim,
    int global_dim,
    int expected_arc_count,
    mespprc_instance_t** out_instance
);

MESPPRC_API void mespprc_instance_destroy(mespprc_instance_t* instance);

MESPPRC_API mespprc_status_t mespprc_instance_set_local_limits(
    mespprc_instance_t* instance,
    const double* limits,
    int dim
);

MESPPRC_API mespprc_status_t mespprc_instance_set_global_limits(
    mespprc_instance_t* instance,
    const double* limits,
    int dim
);

MESPPRC_API mespprc_status_t mespprc_instance_add_node(
    mespprc_instance_t* instance,
    int node_id,
    int node_type
);

MESPPRC_API mespprc_status_t mespprc_instance_add_arc(
    mespprc_instance_t* instance,
    int tail,
    int head,
    double cost,
    const double* local_res,
    const double* global_res
);

MESPPRC_API mespprc_status_t mespprc_instance_finalize(
    mespprc_instance_t* instance
);

/* ---------- Instance accessors ---------- */

MESPPRC_API int mespprc_instance_node_count(const mespprc_instance_t* instance);
MESPPRC_API int mespprc_instance_arc_count(const mespprc_instance_t* instance);
MESPPRC_API int mespprc_instance_local_dim(const mespprc_instance_t* instance);
MESPPRC_API int mespprc_instance_global_dim(const mespprc_instance_t* instance);
MESPPRC_API int mespprc_instance_source_id(const mespprc_instance_t* instance);
MESPPRC_API int mespprc_instance_sink_id(const mespprc_instance_t* instance);
MESPPRC_API int mespprc_instance_is_finalized(const mespprc_instance_t* instance);

/* Read back arc data by its zero-based insertion index. Returns
 * MESPPRC_ERR_INVALID_ARG if `index` is out of range. The output pointers
 * may be NULL to skip individual fields. */
MESPPRC_API mespprc_status_t mespprc_instance_get_arc(
    const mespprc_instance_t* instance,
    int index,
    int* out_tail,
    int* out_head,
    double* out_cost,
    double* out_local_res,
    double* out_global_res
);

/* ---------- Phase 1 (Phase B) ---------- *
 *
 * `label_limit` matches the Python `label_limit` argument: 0 (or any value
 * <= 0) means "no cap." A positive value caps surviving labels per node and
 * is heuristic — Phase 1 may then drop non-dominated labels.
 *
 * The returned result handle owns:
 *   - per-route cost
 *   - per-route path (sequence of node ids)
 *   - per-route local resource vector (length = local_dim)
 *   - per-route global resource vector (length = global_dim)
 *   - per-route first_customer_in_route (-1 if route has no customer visit)
 *   - per-route customer-state signature (length = num_customers)
 *
 * All accessors return MESPPRC_ERR_INVALID_ARG if the result handle is NULL or
 * the index is out of range. Output buffer accessors return
 * MESPPRC_ERR_BUFFER_TOO_SMALL when the caller's buffer is shorter than the
 * value reported by the matching length accessor.
 */

MESPPRC_API mespprc_status_t mespprc_solve_phase1(
    const mespprc_instance_t* instance,
    int label_limit,
    mespprc_phase1_result_t** out_result
);
MESPPRC_API void mespprc_phase1_result_destroy(mespprc_phase1_result_t* result);

MESPPRC_API int mespprc_phase1_route_count(const mespprc_phase1_result_t* result);
MESPPRC_API int mespprc_phase1_num_customers(const mespprc_phase1_result_t* result);
MESPPRC_API int mespprc_phase1_local_dim(const mespprc_phase1_result_t* result);
MESPPRC_API int mespprc_phase1_global_dim(const mespprc_phase1_result_t* result);
MESPPRC_API int mespprc_phase1_path_length(
    const mespprc_phase1_result_t* result, int route_index);

MESPPRC_API mespprc_status_t mespprc_phase1_route_cost(
    const mespprc_phase1_result_t* result, int route_index, double* out_cost);
MESPPRC_API mespprc_status_t mespprc_phase1_route_first_customer(
    const mespprc_phase1_result_t* result, int route_index, int* out_first_customer);
MESPPRC_API mespprc_status_t mespprc_phase1_route_path(
    const mespprc_phase1_result_t* result, int route_index,
    int* out_buffer, int buffer_capacity);
MESPPRC_API mespprc_status_t mespprc_phase1_route_local_resources(
    const mespprc_phase1_result_t* result, int route_index,
    double* out_buffer, int buffer_capacity);
MESPPRC_API mespprc_status_t mespprc_phase1_route_global_resources(
    const mespprc_phase1_result_t* result, int route_index,
    double* out_buffer, int buffer_capacity);
MESPPRC_API mespprc_status_t mespprc_phase1_route_customer_state_signature(
    const mespprc_phase1_result_t* result, int route_index,
    int* out_buffer, int buffer_capacity);

/* ---------- Phase 2 DP (Phase C) ---------- *
 *
 * Status codes returned by `mespprc_phase2_dp_status`:
 *   0 = optimal      (a feasible cover was found)
 *   1 = infeasible
 *
 * Infeasibility reasons returned by `mespprc_phase2_dp_infeasibility_reason`:
 *   0 = none
 *   1 = ROUTE_SET_INFEASIBLE     (the route pool cannot cover required customers)
 *   2 = GLOBAL_LIMITS_INFEASIBLE (cover exists ignoring global limits but not under them)
 *
 * `selected_routes` returns 0-based indices into the original Phase 1 result's
 * route array (i.e. position in `phase1_result->paths`). The Python adapter
 * maps those back to the corresponding Python `Route.route_id`.
 */

#define MESPPRC_PHASE2_STATUS_OPTIMAL    0
#define MESPPRC_PHASE2_STATUS_INFEASIBLE 1

#define MESPPRC_PHASE2_INFEAS_NONE                  0
#define MESPPRC_PHASE2_INFEAS_ROUTE_SET_INFEASIBLE  1
#define MESPPRC_PHASE2_INFEAS_GLOBAL_LIMITS         2

MESPPRC_API mespprc_status_t mespprc_solve_phase2_dp(
    const mespprc_instance_t* instance,
    const mespprc_phase1_result_t* routes,
    mespprc_phase2_dp_result_t** out_result
);
MESPPRC_API void mespprc_phase2_dp_result_destroy(mespprc_phase2_dp_result_t* result);

MESPPRC_API int mespprc_phase2_dp_status(const mespprc_phase2_dp_result_t* result);
MESPPRC_API int mespprc_phase2_dp_infeasibility_reason(
    const mespprc_phase2_dp_result_t* result);
MESPPRC_API int mespprc_phase2_dp_is_feasible(const mespprc_phase2_dp_result_t* result);
MESPPRC_API int mespprc_phase2_dp_coverage_complete(
    const mespprc_phase2_dp_result_t* result);
MESPPRC_API mespprc_status_t mespprc_phase2_dp_total_cost(
    const mespprc_phase2_dp_result_t* result, double* out_cost);
MESPPRC_API int mespprc_phase2_dp_selected_route_count(
    const mespprc_phase2_dp_result_t* result);
MESPPRC_API mespprc_status_t mespprc_phase2_dp_selected_routes(
    const mespprc_phase2_dp_result_t* result,
    int* out_phase1_indices, int buffer_capacity);

/* ---------- Phase 2 IP (Phase D) ---------- *
 *
 * Set-partitioning MIP, identical structure to Python `Phase2IPSolver`:
 *
 *   minimize   sum_i  cost_i * x_i
 *   subject to sum_i  [i covers c]  * x_i == 1   for each required customer c
 *              sum_i  global_res[d][i] * x_i  <= global_limit[d]   for each d
 *              x_i ∈ {0,1}
 *
 * The route pool is first reduced by structural dominance (same predicate as
 * the Python implementation). To distinguish ROUTE_SET vs GLOBAL_LIMITS
 * infeasibility we solve the relaxed model first (no global-resource rows).
 *
 * Backed by HiGHS via its C API (third_party/HiGHS/src/interfaces/highs_c_api.h).
 * Status / infeasibility-reason / accessors mirror Phase 2 DP exactly.
 */

MESPPRC_API mespprc_status_t mespprc_solve_phase2_ip(
    const mespprc_instance_t* instance,
    const mespprc_phase1_result_t* routes,
    mespprc_phase2_ip_result_t** out_result
);
MESPPRC_API void mespprc_phase2_ip_result_destroy(mespprc_phase2_ip_result_t* result);

MESPPRC_API int mespprc_phase2_ip_status(const mespprc_phase2_ip_result_t* result);
MESPPRC_API int mespprc_phase2_ip_infeasibility_reason(
    const mespprc_phase2_ip_result_t* result);
MESPPRC_API int mespprc_phase2_ip_is_feasible(const mespprc_phase2_ip_result_t* result);
MESPPRC_API int mespprc_phase2_ip_coverage_complete(
    const mespprc_phase2_ip_result_t* result);
MESPPRC_API mespprc_status_t mespprc_phase2_ip_total_cost(
    const mespprc_phase2_ip_result_t* result, double* out_cost);
MESPPRC_API int mespprc_phase2_ip_selected_route_count(
    const mespprc_phase2_ip_result_t* result);
MESPPRC_API mespprc_status_t mespprc_phase2_ip_selected_routes(
    const mespprc_phase2_ip_result_t* result,
    int* out_phase1_indices, int buffer_capacity);
MESPPRC_API int mespprc_phase2_ip_original_route_count(
    const mespprc_phase2_ip_result_t* result);
MESPPRC_API int mespprc_phase2_ip_reduced_route_count(
    const mespprc_phase2_ip_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* MESPPRC_H */
