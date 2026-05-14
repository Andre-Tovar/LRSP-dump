/*
 * Unit test for the column data model.
 *
 * Confirms:
 *  - lrsp_column_build sorts covered_customer_ids ascending.
 *  - the same logical column produces the same signature regardless of the
 *    order in which the customers were passed in.
 *  - two columns covering the same customer set but on different facilities
 *    have different signatures.
 *  - lrsp_column_pool_contains finds an inserted signature and rejects an
 *    unknown one.
 *  - the singleton helper produces a column with the expected fields.
 */

#include <stdio.h>

#include "internal.h"

static int fail = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); fail = 1; } \
} while (0)

static lrsp_column_t* build_column(
    lrsp_arena_t* arena, int facility_id,
    const int* dense, const int* ids, int n
) {
    int route_offsets[2] = { 0, n + 2 };
    int route_path[16];
    route_path[0] = 0;
    for (int i = 0; i < n; ++i) route_path[1 + i] = ids[i];
    route_path[n + 1] = ids[n - 1] + 1; /* sentinel sink */
    return lrsp_column_build(
        arena,
        /*column_id=*/100 + facility_id,
        facility_id,
        /*facility_index=*/facility_id,
        dense, ids, n,
        /*pairing_cost=*/12.5,
        /*reduced_cost=*/-1.5,
        /*total_demand=*/30.0,
        /*total_travel_cost=*/8.5,
        route_offsets, route_path, /*route_count=*/1,
        /*iteration=*/0,
        /*kind=*/1);
}

int main(void) {
    lrsp_arena_t* arena = lrsp_arena_create(0);
    ASSERT(arena != NULL, "arena_create");

    /* Two columns with the same covered set but different insertion order
     * should produce identical signatures. */
    int dense_a[3] = { 2, 0, 1 };
    int ids_a[3]   = { 7, 5, 6 };
    int dense_b[3] = { 0, 1, 2 };
    int ids_b[3]   = { 5, 6, 7 };

    lrsp_column_t* a = build_column(arena, /*facility_id=*/4, dense_a, ids_a, 3);
    lrsp_column_t* b = build_column(arena, /*facility_id=*/4, dense_b, ids_b, 3);
    ASSERT(a && b, "build_column returned non-null");
    /* sort produces ascending ids */
    ASSERT(a->covered_customer_ids[0] == 5
        && a->covered_customer_ids[1] == 6
        && a->covered_customer_ids[2] == 7,
        "covered_customer_ids sorted ascending");
    /* signatures match regardless of insertion order */
    /* Note the singleton route differs because of the route_path values; the
     * insertion-order-independent claim only holds when we use the same
     * route_path bytes. We construct that case explicitly below. */
    ASSERT(a->signature != 0, "signature non-zero");
    ASSERT(b->signature != 0, "signature non-zero");

    /* Build two columns with identical facility_id, identical sorted covered
     * customers, identical route paths. Their signatures must match. */
    int dense_x[2] = { 0, 1 };
    int ids_x[2]   = { 5, 6 };
    int route_offsets[2] = { 0, 4 };
    int route_path[4]    = { 0, 5, 6, 99 };
    lrsp_column_t* x = lrsp_column_build(
        arena, 1, 9, 9, dense_x, ids_x, 2,
        1.0, -0.5, 10.0, 5.0,
        route_offsets, route_path, 1,
        0, 1);
    int dense_y[2] = { 1, 0 };
    int ids_y[2]   = { 6, 5 };
    lrsp_column_t* y = lrsp_column_build(
        arena, 2, 9, 9, dense_y, ids_y, 2,
        1.0, -0.5, 10.0, 5.0,
        route_offsets, route_path, 1,
        0, 1);
    ASSERT(x->signature == y->signature,
           "same facility + same sorted coverage + same route path => same signature");

    /* Different facility id changes the signature. */
    lrsp_column_t* z = lrsp_column_build(
        arena, 3, /*facility_id=*/10, 10,
        dense_x, ids_x, 2,
        1.0, -0.5, 10.0, 5.0,
        route_offsets, route_path, 1,
        0, 1);
    ASSERT(z->signature != x->signature,
           "different facility => different signature");

    /* Linked-list pool dedup. */
    x->next = y;
    y->next = z;
    ASSERT(lrsp_column_pool_contains(x, x->signature) == 1, "pool contains x");
    ASSERT(lrsp_column_pool_contains(x, z->signature) == 1, "pool contains z");
    ASSERT(lrsp_column_pool_contains(x, 0xdeadbeefULL) == 0, "pool rejects unknown");

    /* Singleton helper. */
    lrsp_column_t* s = lrsp_column_build_singleton(
        arena, /*column_id=*/777,
        /*facility_id=*/8, /*facility_index=*/0,
        /*customer_id=*/3, /*customer_dense_index=*/0,
        /*sink_node_id=*/99,
        /*pairing_cost=*/20.0, /*total_demand=*/15.0, /*total_travel_cost=*/12.0,
        /*iteration=*/-1);
    ASSERT(s != NULL, "build_singleton non-null");
    ASSERT(s->covered_count == 1, "singleton covered_count=1");
    ASSERT(s->covered_customer_ids[0] == 3, "singleton covers customer 3");
    ASSERT(s->route_count == 1, "singleton has 1 route");
    ASSERT(s->kind == 0, "singleton kind=0 (seed)");

    lrsp_arena_destroy(arena);

    if (fail) {
        fprintf(stderr, "test_column: at least one assertion failed\n");
        return 1;
    }
    printf("test_column: ok\n");
    return 0;
}
