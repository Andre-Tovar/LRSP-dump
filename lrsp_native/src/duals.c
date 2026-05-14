/*
 * Master dual values. Mirrors `lrsp_solver/column.py::MasterDuals`.
 *
 * Sign convention is the one the LP solver returns directly for the rows as
 * they are written:
 *   coverage[i]                   dual of `==` row, any sign
 *   facility_capacity[j]          dual of `<=` row, ≤ 0 when binding
 *   link[i*F + j] (if enabled)    dual of `<=` row, ≤ 0 when binding
 *   min_open_facilities           dual of `>=` row, ≥ 0 when binding
 *
 * The pricing graph (`pricing_graph.c`) subtracts these duals from arc and
 * column costs in the same way the Python code does. We do not flip signs
 * here.
 */

#include "internal.h"

#include <string.h>

lrsp_duals_t* lrsp_duals_create(
    lrsp_arena_t* arena,
    int num_customers,
    int num_facilities,
    int with_link
) {
    if (!arena || num_customers < 0 || num_facilities < 0) return NULL;

    lrsp_duals_t* d = (lrsp_duals_t*)lrsp_arena_calloc(
        arena, sizeof(*d), sizeof(void*));
    if (!d) return NULL;

    d->num_customers  = num_customers;
    d->num_facilities = num_facilities;

    if (num_customers > 0) {
        d->coverage = (double*)lrsp_arena_calloc(
            arena, sizeof(double) * (size_t)num_customers, sizeof(double));
        if (!d->coverage) return NULL;
    }
    if (num_facilities > 0) {
        d->facility_capacity = (double*)lrsp_arena_calloc(
            arena, sizeof(double) * (size_t)num_facilities, sizeof(double));
        if (!d->facility_capacity) return NULL;
    }
    if (with_link && num_customers > 0 && num_facilities > 0) {
        d->link = (double*)lrsp_arena_calloc(
            arena,
            sizeof(double) * (size_t)num_customers * (size_t)num_facilities,
            sizeof(double));
        if (!d->link) return NULL;
    } else {
        d->link = NULL;
    }
    d->min_open_facilities = 0.0;
    return d;
}

void lrsp_duals_zero(lrsp_duals_t* duals) {
    if (!duals) return;
    if (duals->coverage && duals->num_customers > 0) {
        memset(duals->coverage, 0,
               sizeof(double) * (size_t)duals->num_customers);
    }
    if (duals->facility_capacity && duals->num_facilities > 0) {
        memset(duals->facility_capacity, 0,
               sizeof(double) * (size_t)duals->num_facilities);
    }
    if (duals->link && duals->num_customers > 0 && duals->num_facilities > 0) {
        memset(duals->link, 0,
               sizeof(double) * (size_t)duals->num_customers
                              * (size_t)duals->num_facilities);
    }
    duals->min_open_facilities = 0.0;
}
