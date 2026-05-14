/*
 * Pricing adapters. Mirror lrsp_solver/pricing_dp.py and pricing_ip.py.
 *
 * Both engines:
 *   1. Build the per-facility reduced-cost graph.
 *   2. Run Phase 1 (mespprc_solve_phase1) to enumerate elementary routes.
 *      Phase 1's reported `cost` is already the reduced cost because the
 *      pricing graph's arc costs are dual-adjusted.
 *   3. Emit each negative-reduced-cost Phase 1 route as a singleton column
 *      (kind=1).
 *   4. If the LRSP instance has a vehicle_time_limit AND Phase 1 produced
 *      ≥ 2 negative-RC routes, run Phase 2 (DP or IP) on those routes and
 *      emit one pairing column (kind=2) per facility per call.
 *   5. Cap the total emitted columns at config.max_columns_per_facility.
 */

#include "internal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mespprc.h"

/* Helper: build a single Column from a Phase 1 route handle.
 *
 * `phase1_index` is the index into the Phase 1 result. The graph carries the
 * mapping from MESPPRC customer dense index to LRSP customer dense index
 * (they're equal in our construction — both use instance->customers order)
 * and from external ids to base arc costs. */
static lrsp_column_t* build_phase1_column(
    const lrsp_instance_t* instance,
    int facility_dense_index,
    lrsp_pricing_graph_t* graph,
    const mespprc_phase1_result_t* phase1,
    int phase1_index,
    double pairing_constant,
    int column_id,
    int iteration,
    int kind,
    lrsp_arena_t* arena
) {
    /* Path from MESPPRC. */
    int path_len = mespprc_phase1_path_length(phase1, phase1_index);
    if (path_len <= 0) return NULL;
    int* path = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)path_len, sizeof(int));
    if (!path) return NULL;
    mespprc_status_t mst = mespprc_phase1_route_path(phase1, phase1_index, path, path_len);
    if (mst != MESPPRC_OK) return NULL;

    /* Phase 1 reduced cost. */
    double rc = 0.0;
    mst = mespprc_phase1_route_cost(phase1, phase1_index, &rc);
    if (mst != MESPPRC_OK) return NULL;

    /* Customer-state signature: positive entries mean visited. */
    int n_cust = instance->num_customers;
    int* signature = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)n_cust, sizeof(int));
    if (!signature) return NULL;
    mst = mespprc_phase1_route_customer_state_signature(
        phase1, phase1_index, signature, n_cust);
    if (mst != MESPPRC_OK) return NULL;

    int covered_count = 0;
    for (int c = 0; c < n_cust; ++c) if (signature[c] > 0) covered_count++;
    if (covered_count == 0) return NULL;

    int* covered_dense = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)covered_count, sizeof(int));
    int* covered_ids   = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)covered_count, sizeof(int));
    if (!covered_dense || !covered_ids) return NULL;

    int p = 0;
    double total_demand = 0.0;
    for (int c = 0; c < n_cust; ++c) {
        if (signature[c] > 0) {
            covered_dense[p] = c;
            covered_ids[p]   = instance->customers[c].id;
            total_demand    += instance->customers[c].demand;
            p++;
        }
    }

    /* Actual (un-discounted) travel cost via the graph base costs. */
    double travel = lrsp_pricing_graph_actual_route_travel_cost(graph, path, path_len);
    if (!(travel >= 0.0)) return NULL;
    double pairing_cost = instance->vehicle_fixed_cost + travel;
    double reduced_cost = rc + pairing_constant;

    int route_offsets[2] = { 0, path_len };
    int facility_id = instance->facilities[facility_dense_index].id;
    return lrsp_column_build(
        arena,
        column_id,
        facility_id,
        facility_dense_index,
        covered_dense, covered_ids, covered_count,
        pairing_cost,
        reduced_cost,
        total_demand,
        travel,
        route_offsets, path, /*route_count=*/1,
        iteration,
        kind);
}

/* Helper: build a pairing Column from selected Phase 1 indices (Phase 2 output). */
static lrsp_column_t* build_pairing_column(
    const lrsp_instance_t* instance,
    int facility_dense_index,
    lrsp_pricing_graph_t* graph,
    const mespprc_phase1_result_t* phase1,
    const int* phase1_indices,
    int route_count,
    double phase2_objective,
    double pairing_constant,
    int column_id,
    int iteration,
    lrsp_arena_t* arena
) {
    if (route_count <= 0) return NULL;

    /* Concatenate paths via offsets. */
    int total_path_len = 0;
    int* path_lens = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)route_count, sizeof(int));
    if (!path_lens) return NULL;
    for (int r = 0; r < route_count; ++r) {
        int idx = phase1_indices[r];
        path_lens[r] = mespprc_phase1_path_length(phase1, idx);
        if (path_lens[r] <= 0) return NULL;
        total_path_len += path_lens[r];
    }
    int* offsets = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)(route_count + 1), sizeof(int));
    int* paths = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)total_path_len, sizeof(int));
    if (!offsets || !paths) return NULL;

    int n_cust = instance->num_customers;
    int* signature = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)n_cust, sizeof(int));
    int* visited_any = (int*)lrsp_arena_calloc(
        arena, sizeof(int) * (size_t)n_cust, sizeof(int));
    if (!signature || !visited_any) return NULL;

    double total_travel = 0.0;
    int cursor = 0;
    offsets[0] = 0;
    for (int r = 0; r < route_count; ++r) {
        int idx = phase1_indices[r];
        if (mespprc_phase1_route_path(phase1, idx, paths + cursor, path_lens[r])
            != MESPPRC_OK) return NULL;
        if (mespprc_phase1_route_customer_state_signature(
                phase1, idx, signature, n_cust) != MESPPRC_OK) return NULL;
        for (int c = 0; c < n_cust; ++c) if (signature[c] > 0) visited_any[c] = 1;
        double travel_r = lrsp_pricing_graph_actual_route_travel_cost(
            graph, paths + cursor, path_lens[r]);
        if (!(travel_r >= 0.0)) return NULL;
        total_travel += travel_r;
        cursor      += path_lens[r];
        offsets[r + 1] = cursor;
    }

    int covered_count = 0;
    for (int c = 0; c < n_cust; ++c) if (visited_any[c]) covered_count++;
    if (covered_count == 0) return NULL;
    int* covered_dense = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)covered_count, sizeof(int));
    int* covered_ids   = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)covered_count, sizeof(int));
    if (!covered_dense || !covered_ids) return NULL;
    int p = 0;
    double total_demand = 0.0;
    for (int c = 0; c < n_cust; ++c) {
        if (visited_any[c]) {
            covered_dense[p] = c;
            covered_ids[p]   = instance->customers[c].id;
            total_demand    += instance->customers[c].demand;
            p++;
        }
    }

    /* Pairing cost: K * vehicle_fixed_cost + total travel (each route in
     * the pairing pays the per-trip fixed cost). The reduced cost is then
     *
     *     rc = K * vehicle_fixed_cost + (total_travel - duals_sum)
     *        = K * vehicle_fixed_cost + phase2_objective
     *
     * The Python implementation in `_routes_to_pairing_column` only adds one
     * `pairing_constant` (= `vehicle_fixed_cost`); that under-estimates the
     * reduced cost by `(K-1) * vehicle_fixed_cost` and admits non-improving
     * pairing columns the master then rejects (wasted work, not incorrect).
     * The C port uses the correct formula here. */
    double pairing_cost = instance->vehicle_fixed_cost * (double)route_count
                        + total_travel;
    double reduced_cost = phase2_objective
                        + (double)route_count * pairing_constant;

    /* Reject if not improving. */
    if (reduced_cost >= -1e-6) return NULL;

    int facility_id = instance->facilities[facility_dense_index].id;
    return lrsp_column_build(
        arena,
        column_id,
        facility_id,
        facility_dense_index,
        covered_dense, covered_ids, covered_count,
        pairing_cost,
        reduced_cost,
        total_demand,
        total_travel,
        offsets, paths, route_count,
        iteration,
        /*kind=*/2 /* phase2_pairing */);
}

/* The unified pricing entry. `pricing_method` selects DP or IP for the
 * optional Phase 2 pairing call.
 *
 * `arena` owns the returned columns. `next_column_id` is incremented for
 * every column emitted; the caller passes a starting id and reads the count
 * back. */
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
    lrsp_pricing_result_t* out_result
) {
    if (!instance || !duals || !arena || !out_result) return LRSP_ERR_INVALID_ARG;
    if (facility_dense_index < 0
        || facility_dense_index >= instance->num_facilities) {
        return LRSP_ERR_INVALID_ARG;
    }
    memset(out_result, 0, sizeof(*out_result));
    out_result->facility_index = facility_dense_index;
    out_result->facility_id    = instance->facilities[facility_dense_index].id;
    out_result->best_reduced_cost = INFINITY;

    /* Build pricing graph. */
    lrsp_pricing_graph_t* graph = NULL;
    lrsp_status_t st = lrsp_pricing_graph_build(
        instance, facility_dense_index, duals, &graph);
    if (st != LRSP_OK) return st;

    mespprc_instance_t* mi = lrsp_pricing_graph_mespprc(graph);
    double pairing_constant = lrsp_pricing_graph_pairing_constant(graph);

    /* Phase 1. */
    mespprc_phase1_result_t* phase1 = NULL;
    mespprc_status_t mst = mespprc_solve_phase1(mi, phase1_label_limit, &phase1);
    if (mst != MESPPRC_OK) {
        lrsp_pricing_graph_destroy(graph);
        return LRSP_ERR_SOLVER;
    }

    int route_count = mespprc_phase1_route_count(phase1);
    out_result->phase1_route_count = route_count;

    /* Collect routes with reduced cost < -tol, sort by cost ascending. */
    int*    neg_indices = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)(route_count > 0 ? route_count : 1),
        sizeof(int));
    double* neg_costs   = (double*)lrsp_arena_alloc(
        arena, sizeof(double) * (size_t)(route_count > 0 ? route_count : 1),
        sizeof(double));
    if (!neg_indices || !neg_costs) {
        mespprc_phase1_result_destroy(phase1);
        lrsp_pricing_graph_destroy(graph);
        return LRSP_ERR_NOMEM;
    }
    int neg_count = 0;
    for (int i = 0; i < route_count; ++i) {
        double c = 0.0;
        if (mespprc_phase1_route_cost(phase1, i, &c) != MESPPRC_OK) continue;
        if (c < -improvement_tolerance) {
            neg_indices[neg_count] = i;
            neg_costs[neg_count]   = c;
            neg_count++;
        }
    }
    /* Insertion sort by cost ascending. */
    for (int i = 1; i < neg_count; ++i) {
        int ki = neg_indices[i];
        double kc = neg_costs[i];
        int j = i - 1;
        while (j >= 0 && neg_costs[j] > kc) {
            neg_indices[j + 1] = neg_indices[j];
            neg_costs[j + 1]   = neg_costs[j];
            j--;
        }
        neg_indices[j + 1] = ki;
        neg_costs[j + 1]   = kc;
    }
    out_result->phase1_negative_count = neg_count;

    /* Build singleton columns up to max_columns_per_facility. */
    int max_cols = max_columns_per_facility > 0 ? max_columns_per_facility : neg_count;
    int budget = max_cols;
    int budget_used = 0;
    int next_id = next_column_id_start;

    out_result->columns = (lrsp_column_t**)lrsp_arena_calloc(
        arena, sizeof(lrsp_column_t*) * (size_t)(neg_count + 2), sizeof(void*));
    if (!out_result->columns) {
        mespprc_phase1_result_destroy(phase1);
        lrsp_pricing_graph_destroy(graph);
        return LRSP_ERR_NOMEM;
    }

    for (int i = 0; i < neg_count && budget_used < budget; ++i) {
        lrsp_column_t* col = build_phase1_column(
            instance, facility_dense_index, graph, phase1,
            neg_indices[i], pairing_constant,
            next_id++, iteration, /*kind=*/1, arena);
        if (!col) continue;
        out_result->columns[out_result->column_count++] = col;
        if (col->reduced_cost < out_result->best_reduced_cost) {
            out_result->best_reduced_cost = col->reduced_cost;
        }
        budget_used++;
    }

    /* Optional Phase 2 pairing column. */
    int has_time_limit = (instance->vehicle_time_limit >= 0.0) ? 1 : 0;
    int can_try_phase2 = has_time_limit && (neg_count >= 2);

    /* HYBRID is resolved per-run in column_generation.c using a trained
     * per-instance decision tree; by the time we get here pricing_method
     * is always DP or IP. We treat any stray HYBRID as IP (the strong
     * majority class) just in case the caller bypasses the outer loop. */
    lrsp_pricing_method_t resolved_method = pricing_method;
    if (resolved_method == LRSP_PRICING_HYBRID) {
        resolved_method = LRSP_PRICING_IP;
    }

    if (can_try_phase2) {
        if (resolved_method == LRSP_PRICING_DP) {
            mespprc_phase2_dp_result_t* p2 = NULL;
            mespprc_status_t s2 = mespprc_solve_phase2_dp(mi, phase1, &p2);
            if (s2 == MESPPRC_OK && p2 != NULL) {
                if (mespprc_phase2_dp_is_feasible(p2)) {
                    int n_sel = mespprc_phase2_dp_selected_route_count(p2);
                    double obj = 0.0;
                    if (n_sel > 0
                        && mespprc_phase2_dp_total_cost(p2, &obj) == MESPPRC_OK) {
                        int* sel = (int*)lrsp_arena_alloc(
                            arena, sizeof(int) * (size_t)n_sel, sizeof(int));
                        if (sel
                            && mespprc_phase2_dp_selected_routes(p2, sel, n_sel)
                               == MESPPRC_OK) {
                            lrsp_column_t* pcol = build_pairing_column(
                                instance, facility_dense_index, graph,
                                phase1, sel, n_sel,
                                obj, pairing_constant,
                                next_id++, iteration, arena);
                            if (pcol) {
                                out_result->columns[out_result->column_count++] = pcol;
                                if (pcol->reduced_cost < out_result->best_reduced_cost) {
                                    out_result->best_reduced_cost = pcol->reduced_cost;
                                }
                                out_result->pairing_column_added = 1;
                            }
                        }
                    }
                }
                mespprc_phase2_dp_result_destroy(p2);
            }
        } else {
            mespprc_phase2_ip_result_t* p2 = NULL;
            mespprc_status_t s2 = mespprc_solve_phase2_ip(mi, phase1, &p2);
            if (s2 == MESPPRC_OK && p2 != NULL) {
                if (mespprc_phase2_ip_is_feasible(p2)) {
                    int n_sel = mespprc_phase2_ip_selected_route_count(p2);
                    double obj = 0.0;
                    if (n_sel > 0
                        && mespprc_phase2_ip_total_cost(p2, &obj) == MESPPRC_OK) {
                        int* sel = (int*)lrsp_arena_alloc(
                            arena, sizeof(int) * (size_t)n_sel, sizeof(int));
                        if (sel
                            && mespprc_phase2_ip_selected_routes(p2, sel, n_sel)
                               == MESPPRC_OK) {
                            lrsp_column_t* pcol = build_pairing_column(
                                instance, facility_dense_index, graph,
                                phase1, sel, n_sel,
                                obj, pairing_constant,
                                next_id++, iteration, arena);
                            if (pcol) {
                                out_result->columns[out_result->column_count++] = pcol;
                                if (pcol->reduced_cost < out_result->best_reduced_cost) {
                                    out_result->best_reduced_cost = pcol->reduced_cost;
                                }
                                out_result->pairing_column_added = 1;
                            }
                        }
                    }
                }
                mespprc_phase2_ip_result_destroy(p2);
            }
        }
    }

    mespprc_phase1_result_destroy(phase1);
    lrsp_pricing_graph_destroy(graph);
    return LRSP_OK;
}
