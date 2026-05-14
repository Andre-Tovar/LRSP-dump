/*
 * Per-(facility, customer) singleton seed columns. Mirrors
 * lrsp_solver/column_generation.py:178-222.
 *
 * Each seed column visits one customer and returns to the facility. Skipped
 * if demand > vehicle_capacity, or if the round-trip travel cost exceeds the
 * vehicle_time_limit (when the time limit is set).
 */

#include "internal.h"

#include <math.h>
#include <stdlib.h>

int lrsp_build_singleton_warmstart_columns(
    const lrsp_instance_t* instance,
    lrsp_arena_t* arena,
    int* next_column_id,           /* in/out: source/target ids */
    lrsp_column_t*** out_columns,  /* arena-allocated */
    int* out_count
) {
    if (!instance || !arena || !next_column_id || !out_columns || !out_count) {
        return -1;
    }
    *out_columns = NULL;
    *out_count = 0;

    int F = instance->num_facilities;
    int C = instance->num_customers;
    int max_count = F * C;
    if (max_count == 0) return 0;

    lrsp_column_t** cols = (lrsp_column_t**)lrsp_arena_calloc(
        arena, sizeof(lrsp_column_t*) * (size_t)max_count, sizeof(void*));
    if (!cols) return -1;

    /* Sink id used in the route path is max(customer_id) + 1, matching the
     * Python convention. (A column path's exact ids only matter for signature
     * and future reconstruction.) */
    int max_cust_id = 0;
    for (int c = 0; c < C; ++c) {
        if (instance->customers[c].id > max_cust_id) {
            max_cust_id = instance->customers[c].id;
        }
    }
    int sink_id = max_cust_id + 1;

    int p = 0;
    for (int j = 0; j < F; ++j) {
        const lrsp_facility_t* f = &instance->facilities[j];
        for (int ci = 0; ci < C; ++ci) {
            const lrsp_customer_t* c = &instance->customers[ci];
            if (c->demand > instance->vehicle_capacity) continue;
            double d = lrsp_euclidean(f->x, f->y, c->x, c->y);
            double travel = 2.0 * instance->vehicle_operating_cost * d;
            if (instance->vehicle_time_limit >= 0.0
                && travel > instance->vehicle_time_limit) {
                continue;
            }
            double pairing_cost = instance->vehicle_fixed_cost + travel;
            int cid = (*next_column_id)++;
            lrsp_column_t* col = lrsp_column_build_singleton(
                arena, cid, f->id, j,
                c->id, ci, sink_id,
                pairing_cost, c->demand, travel,
                /*iteration=*/-1);
            if (col) cols[p++] = col;
        }
    }
    *out_columns = cols;
    *out_count   = p;
    return 0;
}
