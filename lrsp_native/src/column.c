/*
 * Column data model + signature hashing. Mirrors lrsp_solver/column.py.
 *
 * Columns are arena-allocated (the arena lives on the column pool that owns
 * them). The signature is the 64-bit FNV-1a hash of:
 *
 *   facility_id (4 bytes, little-endian)
 *   covered_customer_ids[]  (sorted, 4 bytes each)
 *   route delimiter        (0xFFFFFFFF)
 *   route_paths[]          (concatenated, 4 bytes each, with end-of-route
 *                           markers 0xFFFFFFFE between routes)
 *
 * That's enough to make two columns with the same facility, same set of
 * covered customers, and same per-route paths collide (which is exactly the
 * Python `Column.signature()` semantics from `lrsp_solver/column.py:55-60`).
 */

#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint64_t lrsp_column_compute_signature(
    int facility_id,
    const int* covered_customer_ids,
    int covered_count,
    const int* route_offsets,    /* route_count + 1 entries; NULL if route_count == 0 */
    const int* route_paths,
    int route_count
) {
    uint64_t h = lrsp_fnv1a_64(&facility_id, sizeof(facility_id), 0);

    for (int k = 0; k < covered_count; ++k) {
        int v = covered_customer_ids[k];
        h = lrsp_fnv1a_64(&v, sizeof(v), h);
    }

    int sentinel = -1;          /* 0xFFFFFFFF as int — separator after coverage */
    h = lrsp_fnv1a_64(&sentinel, sizeof(sentinel), h);

    int route_sentinel = -2;    /* between routes */
    for (int r = 0; r < route_count; ++r) {
        int from = route_offsets[r];
        int to   = route_offsets[r + 1];
        for (int p = from; p < to; ++p) {
            int v = route_paths[p];
            h = lrsp_fnv1a_64(&v, sizeof(v), h);
        }
        if (r + 1 < route_count) {
            h = lrsp_fnv1a_64(&route_sentinel, sizeof(route_sentinel), h);
        }
    }
    return h;
}

/* Build a column inside the given arena. The caller passes already-validated
 * data (so we don't need to validate again here); the column is fully owned
 * by the arena. */
lrsp_column_t* lrsp_column_build(
    lrsp_arena_t* arena,
    int column_id,
    int facility_id,
    int facility_index,
    const int* covered_customers_dense,   /* customer dense indices, unsorted ok */
    const int* covered_customer_ids,      /* external ids matching covered_customers_dense */
    int covered_count,
    double pairing_cost,
    double reduced_cost,
    double total_demand,
    double total_travel_cost,
    const int* route_offsets,             /* route_count + 1 entries */
    const int* route_paths,               /* total path length entries */
    int route_count,
    int iteration,
    int kind
) {
    lrsp_column_t* col = (lrsp_column_t*)lrsp_arena_calloc(
        arena, sizeof(*col), sizeof(void*));
    if (!col) return NULL;
    col->column_id      = column_id;
    col->facility_id    = facility_id;
    col->facility_index = facility_index;
    col->covered_count  = covered_count;
    col->pairing_cost   = pairing_cost;
    col->reduced_cost   = reduced_cost;
    col->total_demand   = total_demand;
    col->total_travel_cost = total_travel_cost;
    col->iteration      = iteration;
    col->kind           = kind;
    col->next           = NULL;

    if (covered_count > 0) {
        col->covered_customers = (int*)lrsp_arena_alloc(
            arena, sizeof(int) * (size_t)covered_count, sizeof(int));
        col->covered_customer_ids = (int*)lrsp_arena_alloc(
            arena, sizeof(int) * (size_t)covered_count, sizeof(int));
        if (!col->covered_customers || !col->covered_customer_ids) return NULL;

        /* Sort by external id ascending so signature is deterministic. */
        int* tmp_ids = (int*)lrsp_arena_alloc(
            arena, sizeof(int) * (size_t)covered_count, sizeof(int));
        int* tmp_dense = (int*)lrsp_arena_alloc(
            arena, sizeof(int) * (size_t)covered_count, sizeof(int));
        if (!tmp_ids || !tmp_dense) return NULL;
        for (int i = 0; i < covered_count; ++i) {
            tmp_ids[i]   = covered_customer_ids[i];
            tmp_dense[i] = covered_customers_dense[i];
        }
        /* Insertion sort so we keep dense indices aligned with sorted ids. */
        for (int i = 1; i < covered_count; ++i) {
            int kid = tmp_ids[i];
            int kdn = tmp_dense[i];
            int j = i - 1;
            while (j >= 0 && tmp_ids[j] > kid) {
                tmp_ids[j + 1]   = tmp_ids[j];
                tmp_dense[j + 1] = tmp_dense[j];
                j--;
            }
            tmp_ids[j + 1]   = kid;
            tmp_dense[j + 1] = kdn;
        }
        memcpy(col->covered_customer_ids, tmp_ids,
               sizeof(int) * (size_t)covered_count);
        memcpy(col->covered_customers,    tmp_dense,
               sizeof(int) * (size_t)covered_count);
    }

    col->route_count = route_count;
    if (route_count > 0) {
        col->route_offsets = (int*)lrsp_arena_alloc(
            arena, sizeof(int) * (size_t)(route_count + 1), sizeof(int));
        if (!col->route_offsets) return NULL;
        memcpy(col->route_offsets, route_offsets,
               sizeof(int) * (size_t)(route_count + 1));

        int total_path = route_offsets[route_count];
        if (total_path > 0) {
            col->route_paths = (int*)lrsp_arena_alloc(
                arena, sizeof(int) * (size_t)total_path, sizeof(int));
            if (!col->route_paths) return NULL;
            memcpy(col->route_paths, route_paths,
                   sizeof(int) * (size_t)total_path);
        }
    }

    col->signature = lrsp_column_compute_signature(
        facility_id,
        col->covered_customer_ids,
        covered_count,
        col->route_offsets,
        col->route_paths,
        route_count
    );
    return col;
}

/* Build a singleton "seed" column for one (facility, customer) pair: visits
 * the customer and returns. Used by the warmstart. The caller is responsible
 * for the source/sink node ids; we record them as 0 and `sink_node_id`. */
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
) {
    int covered_dense[1] = { customer_dense_index };
    int covered_ids[1]   = { customer_id };
    int route_offsets[2] = { 0, 3 };
    int route_path[3]    = { 0, customer_id, sink_node_id };
    return lrsp_column_build(
        arena,
        column_id,
        facility_id,
        facility_index,
        covered_dense, covered_ids, /*covered_count=*/1,
        pairing_cost,
        /*reduced_cost=*/0.0,
        total_demand,
        total_travel_cost,
        route_offsets, route_path, /*route_count=*/1,
        iteration,
        /*kind=*/0  /* seed */
    );
}

/* Linear-search dedup against a small linked column pool. Returns 1 if a
 * column with the same signature is already present. */
int lrsp_column_pool_contains(const lrsp_column_t* head, uint64_t signature) {
    for (const lrsp_column_t* c = head; c != NULL; c = c->next) {
        if (c->signature == signature) return 1;
    }
    return 0;
}
