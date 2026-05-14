/*
 * Library-level entry points: version, layout self-check, status code names.
 * Concrete entry points (instance lifecycle, solver, result) live in their
 * own translation units to keep this file small.
 */

#include <string.h>

#include "internal.h"

#define LRSP_VERSION_STRING "lrsp_native 0.1.0"

void lrsp_struct_sizes(lrsp_struct_sizes_t* out) {
    if (!out) return;
    out->struct_sizes = (uint64_t)sizeof(lrsp_struct_sizes_t);
    out->status_t     = (uint64_t)sizeof(lrsp_status_t);
    out->pointer      = (uint64_t)sizeof(void*);
    out->double_      = (uint64_t)sizeof(double);
    out->int_         = (uint64_t)sizeof(int);
}

const char* lrsp_version(void) {
    return LRSP_VERSION_STRING;
}

const char* lrsp_status_name(int status) {
    switch (status) {
        case LRSP_OK:                  return "OK";
        case LRSP_ERR_NOMEM:           return "NOMEM";
        case LRSP_ERR_INVALID_ARG:     return "INVALID_ARG";
        case LRSP_ERR_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case LRSP_ERR_PARSE:           return "PARSE";
        case LRSP_ERR_INFEASIBLE:      return "INFEASIBLE";
        case LRSP_ERR_SOLVER:          return "SOLVER";
        case LRSP_ERR_NOT_FINALIZED:   return "NOT_FINALIZED";
        case LRSP_ERR_BUFFER_TOO_SMALL:return "BUFFER_TOO_SMALL";
        case LRSP_ERR_FILE_NOT_FOUND:  return "FILE_NOT_FOUND";
        default:                       return "UNKNOWN";
    }
}

void lrsp_solver_config_default(lrsp_solver_config_t* cfg) {
    if (!cfg) return;
    cfg->pricing                       = LRSP_PRICING_DP;
    cfg->max_iterations                = 50;
    cfg->max_columns_per_facility      = 16;
    cfg->phase1_label_limit            = 0;
    cfg->solve_integer_master          = 1;
    cfg->seed_with_singletons          = 1;
    cfg->use_facility_customer_linking = 1;     /* Akca/Python default */
    cfg->use_min_open_facilities_bound = 1;     /* Akca/Python default */
    cfg->improvement_tolerance         = 1e-6;
    cfg->time_limit_seconds            = -1.0;  /* no limit */
    cfg->verbose                       = 0;
}
