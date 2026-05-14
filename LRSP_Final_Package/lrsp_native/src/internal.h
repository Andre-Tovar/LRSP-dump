#ifndef LRSP_INTERNAL_H
#define LRSP_INTERNAL_H

/*
 * Internal types shared across the lrsp_native translation units.
 * Nothing here is part of the public ABI.
 */

#include <stddef.h>
#include <stdint.h>

#include "lrsp.h"

/* ---------- Arena ---------- */

typedef struct lrsp_arena_chunk {
    char*  base;
    size_t size;
    size_t used;
    struct lrsp_arena_chunk* next;
} lrsp_arena_chunk_t;

typedef struct lrsp_arena {
    lrsp_arena_chunk_t* head;
    size_t default_chunk_size;
} lrsp_arena_t;

lrsp_arena_t* lrsp_arena_create(size_t default_chunk_size);
void* lrsp_arena_alloc(lrsp_arena_t* arena, size_t bytes, size_t align);
void* lrsp_arena_calloc(lrsp_arena_t* arena, size_t bytes, size_t align);
void  lrsp_arena_destroy(lrsp_arena_t* arena);

/* Reset all chunks except the first; useful for per-iteration scratch. */
void  lrsp_arena_reset(lrsp_arena_t* arena);

/* ---------- LRSP instance ---------- */

typedef struct {
    int    id;
    double x;
    double y;
    double demand;
} lrsp_customer_t;

typedef struct {
    int    id;
    double x;
    double y;
    double opening_cost;
    double capacity;
} lrsp_facility_t;

struct lrsp_instance {
    char*  name;
    int    num_customers;
    int    num_customer_capacity;
    int    num_facilities;
    int    num_facility_capacity;
    lrsp_customer_t* customers;
    lrsp_facility_t* facilities;

    double vehicle_capacity;
    double vehicle_fixed_cost;
    double vehicle_operating_cost;
    double vehicle_time_limit; /* < 0 means "no limit" */

    int    finalized;
};

/* ---------- Master duals ---------- */

typedef struct {
    int    num_customers;
    int    num_facilities;
    double* coverage;             /* len num_customers, indexed by customer dense idx */
    double* facility_capacity;    /* len num_facilities, indexed by facility dense idx */
    /* For v1 we keep linking off; the field is reserved for future use. */
    double* link;                 /* len num_customers * num_facilities, row-major; NULL if linking off */
    double  min_open_facilities;
} lrsp_duals_t;

/* ---------- Column ---------- */

typedef struct lrsp_column {
    int     column_id;
    int     facility_id;
    int     facility_index;       /* dense index for fast lookup */
    int     covered_count;
    int*    covered_customers;    /* sorted dense customer indices */
    int*    covered_customer_ids; /* sorted external customer ids */
    double  pairing_cost;
    double  reduced_cost;
    double  total_demand;
    double  total_travel_cost;
    int     route_count;
    int*    route_offsets;        /* route_count + 1 entries into route_paths */
    int*    route_paths;          /* concatenated path node ids */
    uint64_t signature;           /* FNV hash of (facility_id, covered_customer_ids, route_paths) */
    int     iteration;
    int     kind;                 /* 0=seed, 1=phase1_route, 2=phase2_pairing */
    /* Backing pointer to the next column in the pool (linked list inside the result arena). */
    struct lrsp_column* next;
} lrsp_column_t;

/* ---------- Column construction (lrsp_native/src/column.c) ---------- */

uint64_t lrsp_column_compute_signature(
    int facility_id,
    const int* covered_customer_ids,
    int covered_count,
    const int* route_offsets,
    const int* route_paths,
    int route_count
);

lrsp_column_t* lrsp_column_build(
    lrsp_arena_t* arena,
    int column_id,
    int facility_id,
    int facility_index,
    const int* covered_customers_dense,
    const int* covered_customer_ids,
    int covered_count,
    double pairing_cost,
    double reduced_cost,
    double total_demand,
    double total_travel_cost,
    const int* route_offsets,
    const int* route_paths,
    int route_count,
    int iteration,
    int kind
);

lrsp_column_t* lrsp_column_build_singleton(
    lrsp_arena_t* arena,
    int column_id,
    int facility_id,
    int facility_index,
    int customer_id,
    int customer_dense_index,
    int sink_node_id,
    double pairing_cost,
    double total_demand,
    double total_travel_cost,
    int iteration
);

int lrsp_column_pool_contains(const lrsp_column_t* head, uint64_t signature);

/* ---------- Master duals (lrsp_native/src/duals.c) ---------- */

lrsp_duals_t* lrsp_duals_create(
    lrsp_arena_t* arena,
    int num_customers,
    int num_facilities,
    int with_link
);
void lrsp_duals_zero(lrsp_duals_t* duals);

/* ---------- Master problem (lrsp_native/src/master.c) ---------- */

typedef struct lrsp_master lrsp_master_t;

typedef struct lrsp_master_solution {
    int     is_optimal;
    int     model_status;
    double  objective;
    double* facility_open_values;       /* len num_facilities */
    double* column_values;              /* len master->columns_count */
    int*    selected_column_indices;    /* indices into master->columns[] with > threshold */
    int     selected_column_count;
    int        has_duals;
    lrsp_duals_t* duals;
} lrsp_master_solution_t;

lrsp_master_t* lrsp_master_create(const lrsp_instance_t* instance,
                                  int use_link, int use_min_open, int verbose);
void           lrsp_master_destroy(lrsp_master_t* m);
int            lrsp_master_column_count(const lrsp_master_t* m);
lrsp_arena_t*  lrsp_master_arena(lrsp_master_t* m);
int            lrsp_master_next_column_id(lrsp_master_t* m);
const lrsp_column_t* lrsp_master_column(const lrsp_master_t* m, int idx);
int            lrsp_master_add_columns(lrsp_master_t* m,
                                       lrsp_column_t** cols, int n);

lrsp_master_solution_t* lrsp_master_solve_lp(
    lrsp_master_t* m, lrsp_arena_t* result_arena);
lrsp_master_solution_t* lrsp_master_solve_ip(
    lrsp_master_t* m, lrsp_arena_t* result_arena);

/* ---------- Singleton warmstart (lrsp_native/src/singleton_warmstart.c) ---------- */

int lrsp_build_singleton_warmstart_columns(
    const lrsp_instance_t* instance,
    lrsp_arena_t* arena,
    int* next_column_id,
    lrsp_column_t*** out_columns,
    int* out_count);

/* ---------- Pricing (lrsp_native/src/pricing.c) ---------- */

typedef struct lrsp_pricing_result {
    int     facility_id;
    int     facility_index;
    int     column_count;
    lrsp_column_t** columns;
    double  pricing_time_seconds;
    double  best_reduced_cost;
    int     phase1_route_count;
    int     phase1_negative_count;
    int     pairing_column_added;
    int     status;
} lrsp_pricing_result_t;

lrsp_status_t lrsp_pricing_solve(
    const lrsp_instance_t* instance,
    int facility_dense_index,
    const lrsp_duals_t* duals,
    lrsp_pricing_method_t pricing_method,
    int iteration,
    int next_column_id_start,
    int max_columns_per_facility,
    int phase1_label_limit,
    double improvement_tolerance,
    lrsp_arena_t* arena,
    lrsp_pricing_result_t* out_result);

/* ---------- Pricing graph (lrsp_native/src/pricing_graph.c) ---------- */

/* Forward declaration of mespprc_instance_t to avoid pulling mespprc.h into
 * every translation unit. */
struct mespprc_instance;

typedef struct lrsp_pricing_graph lrsp_pricing_graph_t;

lrsp_status_t lrsp_pricing_graph_build(
    const lrsp_instance_t* instance,
    int facility_dense_index,
    const lrsp_duals_t* duals,
    lrsp_pricing_graph_t** out_graph);

void lrsp_pricing_graph_destroy(lrsp_pricing_graph_t* g);

int  lrsp_pricing_graph_source(const lrsp_pricing_graph_t* g);
int  lrsp_pricing_graph_sink(const lrsp_pricing_graph_t* g);
double lrsp_pricing_graph_pairing_constant(const lrsp_pricing_graph_t* g);
struct mespprc_instance* lrsp_pricing_graph_mespprc(lrsp_pricing_graph_t* g);
double lrsp_pricing_graph_base_arc_cost(
    const lrsp_pricing_graph_t* g, int tail_id, int head_id);
double lrsp_pricing_graph_actual_route_travel_cost(
    const lrsp_pricing_graph_t* g, const int* path, int path_len);

/* ---------- Iteration summary ---------- */

typedef struct lrsp_iteration_summary {
    int     iteration;
    double  master_objective;
    double  master_time;
    double  pricing_time;
    int     new_column_count;
} lrsp_iteration_summary_t;

/* ---------- Result ---------- */

#define LRSP_RES_STATUS_LP_OPTIMAL       0
#define LRSP_RES_STATUS_ITERATION_LIMIT  1
#define LRSP_RES_STATUS_TIME_LIMIT       2
#define LRSP_RES_STATUS_MASTER_FAILED    3
#define LRSP_RES_STATUS_INCOMPLETE       4
#define LRSP_RES_STATUS_NOT_SOLVED       5

struct lrsp_result {
    int     status;
    char*   pricing_engine;       /* "mespprc_dp" or "mespprc_ip" */
    int     iteration_count;
    int     pricing_call_count;
    double  total_runtime;
    double  master_runtime;
    double  pricing_runtime;
    int     reached_optimality;

    int     has_root_lp;
    double  root_lp_objective;

    int     has_integer;
    double  integer_objective;

    int     iteration_summary_count;
    lrsp_iteration_summary_t* iteration_summaries;

    /* Column pool (compacted into an array at the end of the solve). */
    int     column_count;
    lrsp_column_t** columns;

    /* Open facilities at the integer master. */
    int     open_facility_count;
    int*    open_facility_ids;

    lrsp_arena_t* arena; /* freed on destroy */
};

/* ---------- Helpers ---------- */

/* FNV-1a 64-bit hash over an arbitrary byte buffer. */
uint64_t lrsp_fnv1a_64(const void* data, size_t length, uint64_t seed);

/* Euclidean distance between two points. */
double lrsp_euclidean(double x1, double y1, double x2, double y2);

#endif /* LRSP_INTERNAL_H */
