/*
 * Master problem smoke test.
 *
 * Builds a tiny LRSP (1 facility, 2 customers), adds three hand-crafted
 * columns, solves the LP, and asserts:
 *
 *  - the LP reaches optimality
 *  - the optimal objective matches the hand-computed value
 *  - the column that covers both customers is selected to value 1
 *  - the facility variable y is at the demand-implied lower bound
 *  - the dual sign convention matches Python (== rows: any sign;
 *    <= rows: ≤ 0 when binding)
 *
 * Runs the IP step too and asserts the integer objective is the same
 * (the LP is integer-feasible at the optimum here).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"

static int fail = 0;
#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); fail = 1; } \
} while (0)

#define APPROX(a, b, tol) (fabs((a) - (b)) <= (tol))

int main(void) {
    /* Build a 1-facility / 2-customer instance.
     *   facility f1: opening_cost=10, capacity=100, at (0, 0)
     *   customer  c1: demand=20, at (3, 0)
     *   customer  c2: demand=30, at (0, 4)
     *
     * vehicle capacity=100, fixed cost=10, operating cost=1, time limit=240.
     */
    lrsp_instance_t* inst = NULL;
    lrsp_status_t st = lrsp_instance_create(2, 1, 100.0, 10.0, 1.0, 240.0, &inst);
    ASSERT(st == LRSP_OK && inst, "create instance");
    if (!inst) return 1;

    lrsp_instance_add_customer(inst, 1, 3.0, 0.0, 20.0);
    lrsp_instance_add_customer(inst, 2, 0.0, 4.0, 30.0);
    lrsp_instance_add_facility(inst, 100, 0.0, 0.0, /*opening_cost=*/10.0, /*capacity=*/100.0);
    lrsp_instance_finalize(inst);

    /* Disable linking + min-open so the assertions below (which were
     * derived for the basic coverage+capacity formulation) still hold. */
    lrsp_master_t* m = lrsp_master_create(inst, /*use_link=*/0,
                                          /*use_min_open=*/0, /*verbose=*/0);
    ASSERT(m, "create master");

    /* Build three hand-crafted columns:
     *   col1: covers {c1},        pairing_cost=5,  demand=20
     *   col2: covers {c2},        pairing_cost=8,  demand=30
     *   col3: covers {c1, c2},    pairing_cost=10, demand=50
     *
     * Optimal LP:   λ_3 = 1, y_1 = 0.5, others = 0
     *               objective = 10 * 0.5 + 10 = 15
     * Optimal IP:   λ_3 = 1, y_1 = 1
     *               objective = 10 + 10 = 20
     */
    lrsp_arena_t* tmp = lrsp_arena_create(0);
    int dense_c1[1] = {0}; int ids_c1[1] = {1};
    int dense_c2[1] = {1}; int ids_c2[1] = {2};
    int dense_both[2] = {0, 1}; int ids_both[2] = {1, 2};
    int route_offsets[2] = {0, 3};
    int path_c1[3]   = {0, 1, 99};
    int path_c2[3]   = {0, 2, 99};
    int path_both[4] = {0, 1, 2, 99};
    int route_offsets_2[2] = {0, 4};

    lrsp_column_t* col1 = lrsp_column_build(
        tmp, 0, /*facility_id=*/100, /*facility_index=*/0,
        dense_c1, ids_c1, 1, /*pairing_cost=*/5.0, /*reduced_cost=*/0,
        /*total_demand=*/20.0, /*total_travel_cost=*/3.0,
        route_offsets, path_c1, 1, 0, 1);
    lrsp_column_t* col2 = lrsp_column_build(
        tmp, 0, 100, 0, dense_c2, ids_c2, 1, 8.0, 0, 30.0, 4.0,
        route_offsets, path_c2, 1, 0, 1);
    lrsp_column_t* col3 = lrsp_column_build(
        tmp, 0, 100, 0, dense_both, ids_both, 2, 10.0, 0, 50.0, 7.0,
        route_offsets_2, path_both, 1, 0, 1);
    ASSERT(col1 && col2 && col3, "build cols");

    lrsp_column_t* batch[3] = {col1, col2, col3};
    /* IMPORTANT: the master takes ownership of the columns by appending into
     * its arena. We pass pointers; the master mutates `next` to wire the pool
     * linked list. */
    int added = lrsp_master_add_columns(m, batch, 3);
    ASSERT(added == 3, "all 3 columns added");
    ASSERT(lrsp_master_column_count(m) == 3, "column_count == 3");

    /* Adding a duplicate of col3 should be deduplicated. */
    lrsp_column_t* col3_dup = lrsp_column_build(
        tmp, 0, 100, 0, dense_both, ids_both, 2, 99.0, 0, 50.0, 7.0,
        route_offsets_2, path_both, 1, 0, 1);
    lrsp_column_t* batch2[1] = {col3_dup};
    int added_dup = lrsp_master_add_columns(m, batch2, 1);
    ASSERT(added_dup == 0, "duplicate signature is rejected");

    /* Solve LP. */
    lrsp_arena_t* result_arena = lrsp_arena_create(0);
    lrsp_master_solution_t* lp = lrsp_master_solve_lp(m, result_arena);
    ASSERT(lp && lp->is_optimal, "LP solved to optimality");
    if (lp && lp->is_optimal) {
        printf("LP objective: %.6f\n", lp->objective);
        printf("LP y[0] = %.6f\n", lp->facility_open_values[0]);
        for (int k = 0; k < 3; ++k) {
            printf("LP lambda[%d] = %.6f\n", k, lp->column_values[k]);
        }
        ASSERT(APPROX(lp->objective, 15.0, 1e-6), "LP objective == 15");
        ASSERT(APPROX(lp->facility_open_values[0], 0.5, 1e-6), "y_1 == 0.5");
        ASSERT(APPROX(lp->column_values[2], 1.0, 1e-6), "lambda_3 == 1");
        ASSERT(APPROX(lp->column_values[0], 0.0, 1e-6), "lambda_1 == 0");
        ASSERT(APPROX(lp->column_values[1], 0.0, 1e-6), "lambda_2 == 0");

        ASSERT(lp->has_duals, "LP carries duals");
        if (lp->has_duals) {
            printf("coverage duals: pi[c1]=%.6f pi[c2]=%.6f\n",
                   lp->duals->coverage[0], lp->duals->coverage[1]);
            printf("capacity dual: sigma[f1]=%.6f\n",
                   lp->duals->facility_capacity[0]);
            /* Reduced-cost feasibility at every column (sign convention:
             * c - A'y ≥ 0 for cols at lower bound, ≤ 0 for cols at upper).
             * For col 3 (selected, at upper): π_1 + π_2 + 50*σ_1 ≥ c_3 = 10.
             * For col 1 (slack at 0):           π_1 + 20*σ_1 ≤ c_1 = 5.
             * For col 2 (slack at 0):           π_2 + 30*σ_1 ≤ c_2 = 8.
             * For y_1 (interior at 0.5): -100*σ_1 = c_y = 10  =>  σ_1 = -0.1. */
            double pi1 = lp->duals->coverage[0];
            double pi2 = lp->duals->coverage[1];
            double sg  = lp->duals->facility_capacity[0];
            ASSERT(APPROX(sg, -0.1, 1e-6), "capacity dual == -0.1 (y_1 interior)");
            ASSERT(sg <= 1e-9, "capacity dual sign convention: <= 0 for <= row");
            ASSERT(pi1 + 20.0 * sg <= 5.0 + 1e-6,
                   "col1 reduced-cost feasibility");
            ASSERT(pi2 + 30.0 * sg <= 8.0 + 1e-6,
                   "col2 reduced-cost feasibility");
            ASSERT(pi1 + pi2 + 50.0 * sg >= 10.0 - 1e-6,
                   "col3 reduced-cost feasibility (selected => >= cost)");
        }
    }

    /* Solve IP. */
    lrsp_master_solution_t* ip = lrsp_master_solve_ip(m, result_arena);
    ASSERT(ip && ip->is_optimal, "IP solved to optimality");
    if (ip && ip->is_optimal) {
        printf("IP objective: %.6f\n", ip->objective);
        printf("IP y[0] = %.6f\n", ip->facility_open_values[0]);
        ASSERT(APPROX(ip->objective, 20.0, 1e-6),
               "IP objective == 20 (y_1 forced to 1)");
        ASSERT(APPROX(ip->facility_open_values[0], 1.0, 1e-6), "y_1 == 1 in IP");
        ASSERT(APPROX(ip->column_values[2], 1.0, 1e-6), "lambda_3 == 1 in IP");
    }

    lrsp_arena_destroy(result_arena);
    lrsp_master_destroy(m);
    lrsp_arena_destroy(tmp);
    lrsp_instance_destroy(inst);

    if (fail) {
        fprintf(stderr, "test_master_smoke: at least one assertion failed\n");
        return 1;
    }
    printf("test_master_smoke: ok\n");
    return 0;
}
