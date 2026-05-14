#ifndef LRSP_H
#define LRSP_H

/*
 * lrsp_native public C ABI.
 *
 * Mirrors the architecture of `lrsp_solver/` (Python). The C library calls
 * `mespprc_native` for pricing (Phase 1 + Phase 2 DP/IP) and HiGHS for the
 * restricted master. Memory ownership: the library owns every handle returned
 * by an entry point and the caller releases it through the matching
 * `*_destroy` function.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  #ifdef LRSP_BUILDING_DLL
    #define LRSP_API __declspec(dllexport)
  #else
    #define LRSP_API __declspec(dllimport)
  #endif
#else
  #define LRSP_API __attribute__((visibility("default")))
#endif

/* ---------- Status codes ---------- */

typedef enum {
    LRSP_OK                 = 0,
    LRSP_ERR_NOMEM          = 1,
    LRSP_ERR_INVALID_ARG    = 2,
    LRSP_ERR_NOT_IMPLEMENTED = 3,
    LRSP_ERR_PARSE          = 4,
    LRSP_ERR_INFEASIBLE     = 5,
    LRSP_ERR_SOLVER         = 6,
    LRSP_ERR_NOT_FINALIZED  = 7,
    LRSP_ERR_BUFFER_TOO_SMALL = 8,
    LRSP_ERR_FILE_NOT_FOUND = 9
} lrsp_status_t;

LRSP_API const char* lrsp_status_name(int status);

/* ---------- Pricing method enum ---------- */

typedef enum {
    LRSP_PRICING_DP = 0,
    LRSP_PRICING_IP = 1,
    /* Adaptive: inside each pricing call, Phase 1 always runs first.
     * Once Phase 1 produces its negative-reduced-cost route set, the
     * selector dispatches Phase 2 DP or IP using THIS CALL's route
     * count as the load-bearing feature:
     *
     *   if neg_route_count_this_call <= LRSP_HYBRID_PHASE1_THRESHOLD:
     *       use DP   (small pool — DP's label set stays bounded)
     *   else:
     *       use IP   (large pool — DP's exponential bites)
     *
     * T = 3.17 was retrained on the dense sweep
     * (results/lrsp_dp_vs_ip_dense/cells.csv) for no false negatives —
     * the largest value of the Phase-1 routes-per-call feature for
     * which IP never wins over DP in training. This biases the hybrid
     * toward "if uncertain, pick IP," the asymmetric-cost choice when a
     * DP-loss is a timeout and IP-overhead is milliseconds.
     */
    LRSP_PRICING_HYBRID = 2
} lrsp_pricing_method_t;

/* Threshold value used by LRSP_PRICING_HYBRID. Trained on the dense sweep
 * (results/lrsp_dp_vs_ip_dense/cells.csv) for **zero false negatives** —
 * the largest T for which no DP pick is wrong in training. 90.1% in-sample
 * accuracy vs 89.4% always-IP baseline.
 *
 * Override at compile time if you retrain on a different distribution. */
#ifndef LRSP_HYBRID_PHASE1_THRESHOLD
#define LRSP_HYBRID_PHASE1_THRESHOLD 3.17
#endif

/* Backwards-compat alias for the older total-columns-based threshold.
 * Not used by the current selector but kept for documentation. */
#ifndef LRSP_HYBRID_POOL_THRESHOLD
#define LRSP_HYBRID_POOL_THRESHOLD 4.83
#endif

/* ---------- Opaque handles ---------- */

typedef struct lrsp_instance lrsp_instance_t;
typedef struct lrsp_result lrsp_result_t;

/* ---------- Layout self-check ---------- *
 *
 * The Python ctypes binding mirrors the public structs that it dereferences.
 * `lrsp_struct_sizes` reports `sizeof()` for every such struct so the binding
 * can fail fast at import time if the C and Python sides drift.
 */

typedef struct lrsp_struct_sizes {
    uint64_t struct_sizes;
    uint64_t status_t;
    uint64_t pointer;
    uint64_t double_;
    uint64_t int_;
} lrsp_struct_sizes_t;

LRSP_API void lrsp_struct_sizes(lrsp_struct_sizes_t* out);
LRSP_API const char* lrsp_version(void);

/* ---------- Instance construction ---------- *
 *
 * Mirrors `lrsp_solver/instance.py`. The instance holds customers, facilities,
 * vehicle parameters, and (optionally) a vehicle time limit that drives
 * scheduling. Builders are intentionally explicit: the caller adds customers
 * one by one then facilities one by one, then calls `finalize`.
 *
 * `vehicle_time_limit` < 0 means "no limit" (matches `vehicle_time_limit is
 * None` on the Python side).
 */

LRSP_API lrsp_status_t lrsp_instance_create(
    int num_customers,
    int num_facilities,
    double vehicle_capacity,
    double vehicle_fixed_cost,
    double vehicle_operating_cost,
    double vehicle_time_limit,    /* < 0 means no limit */
    lrsp_instance_t** out_instance
);

LRSP_API void lrsp_instance_destroy(lrsp_instance_t* instance);

LRSP_API lrsp_status_t lrsp_instance_add_customer(
    lrsp_instance_t* instance,
    int id,
    double x,
    double y,
    double demand
);

LRSP_API lrsp_status_t lrsp_instance_add_facility(
    lrsp_instance_t* instance,
    int id,
    double x,
    double y,
    double opening_cost,
    double capacity
);

LRSP_API lrsp_status_t lrsp_instance_set_name(
    lrsp_instance_t* instance,
    const char* name
);

LRSP_API lrsp_status_t lrsp_instance_finalize(lrsp_instance_t* instance);

/* ---------- Instance accessors ---------- */

LRSP_API int    lrsp_instance_num_customers(const lrsp_instance_t* instance);
LRSP_API int    lrsp_instance_num_facilities(const lrsp_instance_t* instance);
LRSP_API double lrsp_instance_vehicle_capacity(const lrsp_instance_t* instance);
LRSP_API double lrsp_instance_vehicle_fixed_cost(const lrsp_instance_t* instance);
LRSP_API double lrsp_instance_vehicle_operating_cost(const lrsp_instance_t* instance);
LRSP_API double lrsp_instance_vehicle_time_limit(const lrsp_instance_t* instance); /* < 0 = no limit */
LRSP_API int    lrsp_instance_is_finalized(const lrsp_instance_t* instance);
LRSP_API const char* lrsp_instance_name(const lrsp_instance_t* instance);

LRSP_API lrsp_status_t lrsp_instance_get_customer(
    const lrsp_instance_t* instance, int index,
    int* out_id, double* out_x, double* out_y, double* out_demand
);
LRSP_API lrsp_status_t lrsp_instance_get_facility(
    const lrsp_instance_t* instance, int index,
    int* out_id, double* out_x, double* out_y,
    double* out_opening_cost, double* out_capacity
);

/* ---------- Akca .txt loader ---------- */

LRSP_API lrsp_status_t lrsp_instance_load_akca_txt(
    const char* path,
    double vehicle_operating_cost,
    lrsp_instance_t** out_instance
);

/* ---------- Solver configuration ---------- */

typedef struct lrsp_solver_config {
    lrsp_pricing_method_t pricing;
    int max_iterations;
    int max_columns_per_facility;
    int phase1_label_limit;       /* 0 = no cap */
    int solve_integer_master;     /* 1/0 */
    int seed_with_singletons;     /* 1/0 */
    int use_facility_customer_linking;  /* 1/0; v1 default 0 */
    int use_min_open_facilities_bound;  /* 1/0; v1 default 0 */
    double improvement_tolerance;
    double time_limit_seconds;    /* < 0 = no limit */
    int verbose;                  /* 1/0 */
} lrsp_solver_config_t;

LRSP_API void lrsp_solver_config_default(lrsp_solver_config_t* cfg);

/* ---------- Top-level solve ---------- */

LRSP_API lrsp_status_t lrsp_solve(
    const lrsp_instance_t* instance,
    const lrsp_solver_config_t* config,
    lrsp_result_t** out_result
);

LRSP_API void lrsp_result_destroy(lrsp_result_t* result);

/* ---------- Result accessors ---------- */

/* Termination status code:
 *   0 = lp_optimal (CG converged on the root LP)
 *   1 = iteration_limit
 *   2 = time_limit
 *   3 = master_failed
 *   4 = incomplete
 *   5 = not_solved
 */
LRSP_API int lrsp_result_status(const lrsp_result_t* result);
LRSP_API const char* lrsp_result_status_name(const lrsp_result_t* result);
LRSP_API const char* lrsp_result_pricing_engine(const lrsp_result_t* result);
LRSP_API int    lrsp_result_iteration_count(const lrsp_result_t* result);
LRSP_API int    lrsp_result_pricing_call_count(const lrsp_result_t* result);
LRSP_API int    lrsp_result_column_count(const lrsp_result_t* result);
LRSP_API double lrsp_result_total_runtime(const lrsp_result_t* result);
LRSP_API double lrsp_result_master_runtime(const lrsp_result_t* result);
LRSP_API double lrsp_result_pricing_runtime(const lrsp_result_t* result);
LRSP_API int    lrsp_result_reached_optimality(const lrsp_result_t* result);

/* Returns 1 if root LP is available (optimal at the end of CG), 0 otherwise. */
LRSP_API int    lrsp_result_has_root_lp(const lrsp_result_t* result);
LRSP_API double lrsp_result_root_lp_objective(const lrsp_result_t* result);

LRSP_API int    lrsp_result_has_integer(const lrsp_result_t* result);
LRSP_API double lrsp_result_integer_objective(const lrsp_result_t* result);

/* Per-iteration timing accessors (index 0 .. iteration_count-1). */
LRSP_API lrsp_status_t lrsp_result_iteration_master_time(
    const lrsp_result_t* result, int iteration, double* out);
LRSP_API lrsp_status_t lrsp_result_iteration_pricing_time(
    const lrsp_result_t* result, int iteration, double* out);
LRSP_API lrsp_status_t lrsp_result_iteration_master_objective(
    const lrsp_result_t* result, int iteration, double* out);
LRSP_API lrsp_status_t lrsp_result_iteration_new_columns(
    const lrsp_result_t* result, int iteration, int* out);

/* Per-column accessors (index 0 .. column_count-1). */
LRSP_API lrsp_status_t lrsp_result_column_facility_id(
    const lrsp_result_t* result, int column_index, int* out);
LRSP_API lrsp_status_t lrsp_result_column_pairing_cost(
    const lrsp_result_t* result, int column_index, double* out);
LRSP_API lrsp_status_t lrsp_result_column_total_demand(
    const lrsp_result_t* result, int column_index, double* out);
LRSP_API int lrsp_result_column_covered_count(
    const lrsp_result_t* result, int column_index);
LRSP_API lrsp_status_t lrsp_result_column_covered_customers(
    const lrsp_result_t* result, int column_index,
    int* out_buffer, int buffer_capacity);

/* Column origin code:
 *   0 = seed singleton (warmstart)
 *   1 = phase1 route
 *   2 = phase2 pairing
 */
LRSP_API lrsp_status_t lrsp_result_column_kind(
    const lrsp_result_t* result, int column_index, int* out);
LRSP_API int lrsp_result_column_route_count(
    const lrsp_result_t* result, int column_index);

/* Number of facilities open in the integer master (0 if not solved). */
LRSP_API int lrsp_result_open_facility_count(const lrsp_result_t* result);
LRSP_API lrsp_status_t lrsp_result_open_facility_ids(
    const lrsp_result_t* result, int* out_buffer, int buffer_capacity);

#ifdef __cplusplus
}
#endif

#endif /* LRSP_H */
