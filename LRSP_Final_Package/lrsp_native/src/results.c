/*
 * Result handle accessors and lifecycle. Mirrors lrsp_solver/results.py.
 *
 * The result struct is allocated inside its own arena during the solve; we
 * free that arena on destroy. All array-shaped accessors copy out into the
 * caller's buffer rather than handing out raw pointers, so the Python ctypes
 * binding can stay stable across struct layout changes.
 */

#include "internal.h"

#include <string.h>

void lrsp_result_destroy(lrsp_result_t* result) {
    if (!result) return;
    lrsp_arena_t* arena = result->arena;
    if (arena) lrsp_arena_destroy(arena);
}

int lrsp_result_status(const lrsp_result_t* r) { return r ? r->status : -1; }

const char* lrsp_result_status_name(const lrsp_result_t* r) {
    if (!r) return "(null)";
    switch (r->status) {
        case LRSP_RES_STATUS_LP_OPTIMAL:      return "lp_optimal";
        case LRSP_RES_STATUS_ITERATION_LIMIT: return "iteration_limit";
        case LRSP_RES_STATUS_TIME_LIMIT:      return "time_limit";
        case LRSP_RES_STATUS_MASTER_FAILED:   return "master_failed";
        case LRSP_RES_STATUS_INCOMPLETE:      return "incomplete";
        case LRSP_RES_STATUS_NOT_SOLVED:      return "not_solved";
        default:                              return "unknown";
    }
}

const char* lrsp_result_pricing_engine(const lrsp_result_t* r) {
    if (!r || !r->pricing_engine) return "";
    return r->pricing_engine;
}

int    lrsp_result_iteration_count(const lrsp_result_t* r) {
    return r ? r->iteration_count : 0;
}
int    lrsp_result_pricing_call_count(const lrsp_result_t* r) {
    return r ? r->pricing_call_count : 0;
}
int    lrsp_result_column_count(const lrsp_result_t* r) {
    return r ? r->column_count : 0;
}
double lrsp_result_total_runtime(const lrsp_result_t* r) {
    return r ? r->total_runtime : 0.0;
}
double lrsp_result_master_runtime(const lrsp_result_t* r) {
    return r ? r->master_runtime : 0.0;
}
double lrsp_result_pricing_runtime(const lrsp_result_t* r) {
    return r ? r->pricing_runtime : 0.0;
}
int    lrsp_result_reached_optimality(const lrsp_result_t* r) {
    return r ? r->reached_optimality : 0;
}
int    lrsp_result_has_root_lp(const lrsp_result_t* r) {
    return r ? r->has_root_lp : 0;
}
double lrsp_result_root_lp_objective(const lrsp_result_t* r) {
    return r ? r->root_lp_objective : 0.0;
}
int    lrsp_result_has_integer(const lrsp_result_t* r) {
    return r ? r->has_integer : 0;
}
double lrsp_result_integer_objective(const lrsp_result_t* r) {
    return r ? r->integer_objective : 0.0;
}

lrsp_status_t lrsp_result_iteration_master_time(
    const lrsp_result_t* r, int it, double* out
) {
    if (!r || !out) return LRSP_ERR_INVALID_ARG;
    if (it < 0 || it >= r->iteration_count) return LRSP_ERR_INVALID_ARG;
    *out = r->iteration_summaries[it].master_time;
    return LRSP_OK;
}
lrsp_status_t lrsp_result_iteration_pricing_time(
    const lrsp_result_t* r, int it, double* out
) {
    if (!r || !out) return LRSP_ERR_INVALID_ARG;
    if (it < 0 || it >= r->iteration_count) return LRSP_ERR_INVALID_ARG;
    *out = r->iteration_summaries[it].pricing_time;
    return LRSP_OK;
}
lrsp_status_t lrsp_result_iteration_master_objective(
    const lrsp_result_t* r, int it, double* out
) {
    if (!r || !out) return LRSP_ERR_INVALID_ARG;
    if (it < 0 || it >= r->iteration_count) return LRSP_ERR_INVALID_ARG;
    *out = r->iteration_summaries[it].master_objective;
    return LRSP_OK;
}
lrsp_status_t lrsp_result_iteration_new_columns(
    const lrsp_result_t* r, int it, int* out
) {
    if (!r || !out) return LRSP_ERR_INVALID_ARG;
    if (it < 0 || it >= r->iteration_count) return LRSP_ERR_INVALID_ARG;
    *out = r->iteration_summaries[it].new_column_count;
    return LRSP_OK;
}

lrsp_status_t lrsp_result_column_facility_id(
    const lrsp_result_t* r, int idx, int* out
) {
    if (!r || !out || idx < 0 || idx >= r->column_count) return LRSP_ERR_INVALID_ARG;
    if (!r->columns[idx]) return LRSP_ERR_INVALID_ARG;
    *out = r->columns[idx]->facility_id;
    return LRSP_OK;
}
lrsp_status_t lrsp_result_column_pairing_cost(
    const lrsp_result_t* r, int idx, double* out
) {
    if (!r || !out || idx < 0 || idx >= r->column_count) return LRSP_ERR_INVALID_ARG;
    if (!r->columns[idx]) return LRSP_ERR_INVALID_ARG;
    *out = r->columns[idx]->pairing_cost;
    return LRSP_OK;
}
lrsp_status_t lrsp_result_column_total_demand(
    const lrsp_result_t* r, int idx, double* out
) {
    if (!r || !out || idx < 0 || idx >= r->column_count) return LRSP_ERR_INVALID_ARG;
    if (!r->columns[idx]) return LRSP_ERR_INVALID_ARG;
    *out = r->columns[idx]->total_demand;
    return LRSP_OK;
}
int lrsp_result_column_covered_count(
    const lrsp_result_t* r, int idx
) {
    if (!r || idx < 0 || idx >= r->column_count) return 0;
    if (!r->columns[idx]) return 0;
    return r->columns[idx]->covered_count;
}
lrsp_status_t lrsp_result_column_covered_customers(
    const lrsp_result_t* r, int idx, int* out, int cap
) {
    if (!r || !out || idx < 0 || idx >= r->column_count) return LRSP_ERR_INVALID_ARG;
    const lrsp_column_t* c = r->columns[idx];
    if (!c) return LRSP_ERR_INVALID_ARG;
    if (cap < c->covered_count) return LRSP_ERR_BUFFER_TOO_SMALL;
    for (int k = 0; k < c->covered_count; ++k) out[k] = c->covered_customer_ids[k];
    return LRSP_OK;
}

lrsp_status_t lrsp_result_column_kind(
    const lrsp_result_t* r, int idx, int* out
) {
    if (!r || !out || idx < 0 || idx >= r->column_count) return LRSP_ERR_INVALID_ARG;
    if (!r->columns[idx]) return LRSP_ERR_INVALID_ARG;
    *out = r->columns[idx]->kind;
    return LRSP_OK;
}

int lrsp_result_column_route_count(const lrsp_result_t* r, int idx) {
    if (!r || idx < 0 || idx >= r->column_count) return 0;
    if (!r->columns[idx]) return 0;
    return r->columns[idx]->route_count;
}

int lrsp_result_open_facility_count(const lrsp_result_t* r) {
    return r ? r->open_facility_count : 0;
}
lrsp_status_t lrsp_result_open_facility_ids(
    const lrsp_result_t* r, int* out, int cap
) {
    if (!r || !out) return LRSP_ERR_INVALID_ARG;
    if (cap < r->open_facility_count) return LRSP_ERR_BUFFER_TOO_SMALL;
    for (int k = 0; k < r->open_facility_count; ++k) {
        out[k] = r->open_facility_ids[k];
    }
    return LRSP_OK;
}
