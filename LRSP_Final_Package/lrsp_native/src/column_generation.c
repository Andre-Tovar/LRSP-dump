/*
 * LRSP column-generation outer loop. Mirrors
 * lrsp_solver/column_generation.py:52-175 step-by-step.
 *
 * Each iteration:
 *   1. Solve master LP (continuous), pull duals
 *   2. For each facility: call pricing → get columns
 *   3. Add columns to master (with dedup by signature)
 *   4. Stop on no-new-columns / iteration cap / time limit
 *
 * After CG terminates, optionally re-solve as IP for the final integer
 * objective. The CG loop is the only place that owns the master and the
 * per-iteration scratch arena; the result the caller gets back lives in its
 * own arena that the result handle owns.
 */

#include "internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Per-instance hybrid engine selector. Implements the depth-3 decision
 * tree trained for 100% in-sample accuracy on the 600 s dense sweep
 * (234 cells). Splits on:
 *   - N (number of customers)
 *   - vehicle_time_limit (separates moderate/tight from easy at N=5)
 *   - total_demand (separates easy_s1 from easy_s2/s3 at N=5)
 *
 * Rule:
 *   if N <= 5:
 *     if vehicle_time_limit <= 437.6357: DP
 *     elif total_demand > 108:           DP
 *     else:                              IP
 *   else:                                IP
 *
 * Training script: lrsp_native/scripts/train_hybrid_tree.py
 * Data: results/lrsp_dp_vs_ip_dense_600s/cells.csv */
static lrsp_pricing_method_t lrsp_hybrid_select_engine(
    const lrsp_instance_t* instance
) {
    if (instance->num_customers > 5) return LRSP_PRICING_IP;
    if (instance->vehicle_time_limit <= 437.6357) return LRSP_PRICING_DP;
    double total_demand = 0.0;
    for (int i = 0; i < instance->num_customers; ++i) {
        total_demand += instance->customers[i].demand;
    }
    return (total_demand > 108.0) ? LRSP_PRICING_DP : LRSP_PRICING_IP;
}

/* The result struct exposed by lrsp_solve. It owns its own arena. */
lrsp_status_t lrsp_run_column_generation(
    const lrsp_instance_t* instance,
    const lrsp_solver_config_t* config,
    lrsp_result_t** out_result
) {
    if (!instance || !config || !out_result) return LRSP_ERR_INVALID_ARG;
    if (!instance->finalized) return LRSP_ERR_NOT_FINALIZED;
    *out_result = NULL;

    /* Scratch arena (per-iteration column allocations live here too — the
     * master keeps its own arena for the long-lived column pool). The result
     * arena is separate so the result outlives CG. */
    lrsp_arena_t* scratch = lrsp_arena_create(0);
    if (!scratch) return LRSP_ERR_NOMEM;

    lrsp_arena_t* result_arena = lrsp_arena_create(0);
    if (!result_arena) { lrsp_arena_destroy(scratch); return LRSP_ERR_NOMEM; }

    lrsp_result_t* res = (lrsp_result_t*)lrsp_arena_calloc(
        result_arena, sizeof(*res), sizeof(void*));
    if (!res) {
        lrsp_arena_destroy(result_arena); lrsp_arena_destroy(scratch);
        return LRSP_ERR_NOMEM;
    }
    res->arena = result_arena;
    res->status = LRSP_RES_STATUS_NOT_SOLVED;

    /* Resolve HYBRID to a concrete engine ONCE for the entire run using
     * the per-instance decision tree. All pricing calls in this run will
     * use the chosen engine; no per-call switching. */
    lrsp_pricing_method_t resolved_pricing = config->pricing;
    if (resolved_pricing == LRSP_PRICING_HYBRID) {
        resolved_pricing = lrsp_hybrid_select_engine(instance);
    }

    /* Pricing engine label (cheap allocation). */
    const char* engine_name;
    switch (config->pricing) {
        case LRSP_PRICING_DP:     engine_name = "mespprc_dp";     break;
        case LRSP_PRICING_IP:     engine_name = "mespprc_ip";     break;
        case LRSP_PRICING_HYBRID:
            engine_name = (resolved_pricing == LRSP_PRICING_DP)
                          ? "mespprc_hybrid_dp"
                          : "mespprc_hybrid_ip";
            break;
        default:                  engine_name = "mespprc_unknown"; break;
    }
    size_t engine_len = strlen(engine_name);
    res->pricing_engine = (char*)lrsp_arena_alloc(
        result_arena, engine_len + 1, 1);
    if (!res->pricing_engine) {
        lrsp_arena_destroy(result_arena); lrsp_arena_destroy(scratch);
        return LRSP_ERR_NOMEM;
    }
    memcpy(res->pricing_engine, engine_name, engine_len + 1);

    double run_start = now_seconds();

    /* Build master + warmstart. */
    lrsp_master_t* m = lrsp_master_create(
        instance,
        config->use_facility_customer_linking,
        config->use_min_open_facilities_bound,
        config->verbose);
    if (!m) {
        lrsp_arena_destroy(result_arena); lrsp_arena_destroy(scratch);
        return LRSP_ERR_SOLVER;
    }
    lrsp_arena_t* master_arena = lrsp_master_arena(m);

    if (config->seed_with_singletons) {
        int next_id = lrsp_master_next_column_id(m);
        lrsp_column_t** seeds = NULL;
        int seed_count = 0;
        if (lrsp_build_singleton_warmstart_columns(
                instance, master_arena, &next_id, &seeds, &seed_count) == 0) {
            lrsp_master_add_columns(m, seeds, seed_count);
        }
    }

    int F = instance->num_facilities;
    int max_iters = config->max_iterations > 0 ? config->max_iterations : 50;

    /* Iteration summary array, sized to the cap. */
    res->iteration_summaries = (lrsp_iteration_summary_t*)lrsp_arena_calloc(
        result_arena, sizeof(lrsp_iteration_summary_t) * (size_t)max_iters,
        sizeof(double));

    int hit_time_limit = 0;
    int reached_optimality = 0;
    double last_master_objective = NAN;

    for (int iter = 0; iter < max_iters; ++iter) {
        if (config->time_limit_seconds > 0.0
            && (now_seconds() - run_start) >= config->time_limit_seconds) {
            hit_time_limit = 1;
            break;
        }

        /* 1. Master LP. */
        double t0 = now_seconds();
        lrsp_arena_reset(scratch);  /* recycle per-iteration scratch */
        lrsp_master_solution_t* lp = lrsp_master_solve_lp(m, scratch);
        double master_elapsed = now_seconds() - t0;
        res->master_runtime += master_elapsed;

        if (!lp || !lp->is_optimal || !lp->has_duals) {
            res->status = LRSP_RES_STATUS_MASTER_FAILED;
            break;
        }
        last_master_objective = lp->objective;

        /* 2. Pricing per facility.
         *
         * For LRSP_PRICING_HYBRID the per-call DP/IP decision lives
         * inside lrsp_pricing_solve, where the THIS-CALL Phase 1
         * negative-RC route count is already known before the Phase 2
         * branch. The outer loop just forwards LRSP_PRICING_HYBRID. */
        double iter_pricing_time = 0.0;
        int new_columns_total = 0;
        for (int j = 0; j < F; ++j) {
            lrsp_pricing_result_t pr;
            int next_id = lrsp_master_next_column_id(m);
            double tp0 = now_seconds();
            lrsp_status_t pst = lrsp_pricing_solve(
                instance, j, lp->duals,
                resolved_pricing, iter, next_id,
                config->max_columns_per_facility,
                config->phase1_label_limit,
                config->improvement_tolerance,
                /*arena=*/master_arena,    /* columns live in the master pool */
                &pr);
            iter_pricing_time += now_seconds() - tp0;
            res->pricing_call_count++;
            if (pst != LRSP_OK) continue;
            if (pr.column_count > 0) {
                int added = lrsp_master_add_columns(m, pr.columns, pr.column_count);
                new_columns_total += added;
            }
        }
        res->pricing_runtime += iter_pricing_time;

        /* 3. Iteration summary. */
        if (iter < max_iters && res->iteration_summaries) {
            lrsp_iteration_summary_t* s = &res->iteration_summaries[iter];
            s->iteration         = iter;
            s->master_objective  = lp->objective;
            s->master_time       = master_elapsed;
            s->pricing_time      = iter_pricing_time;
            s->new_column_count  = new_columns_total;
        }
        res->iteration_count = iter + 1;

        if (new_columns_total == 0) {
            reached_optimality = 1;
            res->status = LRSP_RES_STATUS_LP_OPTIMAL;
            break;
        }

        if (config->time_limit_seconds > 0.0
            && (now_seconds() - run_start) >= config->time_limit_seconds) {
            hit_time_limit = 1;
            break;
        }
    }

    if (!reached_optimality && res->status == LRSP_RES_STATUS_NOT_SOLVED) {
        if (hit_time_limit) {
            res->status = LRSP_RES_STATUS_TIME_LIMIT;
        } else if (res->iteration_count >= max_iters) {
            res->status = LRSP_RES_STATUS_ITERATION_LIMIT;
        } else {
            res->status = LRSP_RES_STATUS_INCOMPLETE;
        }
    }

    res->reached_optimality = reached_optimality;

    /* Final LP re-solve to capture every column added in the last pricing
     * round (matches lrsp_solver/column_generation.py:155-160). */
    if (lrsp_master_column_count(m) > 0) {
        double tf = now_seconds();
        lrsp_arena_reset(scratch);
        lrsp_master_solution_t* final_lp = lrsp_master_solve_lp(m, scratch);
        res->master_runtime += now_seconds() - tf;
        if (final_lp && final_lp->is_optimal) {
            res->root_lp_objective = final_lp->objective;
            res->has_root_lp = 1;
            last_master_objective = final_lp->objective;
        }
    }

    /* Optional: solve as IP for the integer objective. */
    if (config->solve_integer_master && lrsp_master_column_count(m) > 0) {
        lrsp_arena_reset(scratch);
        lrsp_master_solution_t* ip = lrsp_master_solve_ip(m, scratch);
        if (ip && ip->is_optimal) {
            res->integer_objective = ip->objective;
            res->has_integer = 1;
            /* Open facilities. */
            int open = 0;
            for (int j = 0; j < F; ++j) {
                if (ip->facility_open_values[j] > 0.5) open++;
            }
            res->open_facility_count = open;
            res->open_facility_ids = (int*)lrsp_arena_alloc(
                result_arena, sizeof(int) * (size_t)(open > 0 ? open : 1),
                sizeof(int));
            if (res->open_facility_ids) {
                int p = 0;
                for (int j = 0; j < F; ++j) {
                    if (ip->facility_open_values[j] > 0.5) {
                        res->open_facility_ids[p++] = instance->facilities[j].id;
                    }
                }
            }
        }
    }

    /* Snapshot the column pool into the result arena. The columns themselves
     * live in master_arena (which dies with the master), so we copy field
     * references — but the simpler approach is to clone each column into the
     * result arena. We do that here. */
    int total_cols = lrsp_master_column_count(m);
    res->columns = (lrsp_column_t**)lrsp_arena_calloc(
        result_arena,
        sizeof(lrsp_column_t*) * (size_t)(total_cols > 0 ? total_cols : 1),
        sizeof(void*));
    if (res->columns && total_cols > 0) {
        for (int k = 0; k < total_cols; ++k) {
            const lrsp_column_t* src = lrsp_master_column(m, k);
            if (!src) continue;
            int total_path = src->route_count > 0
                ? src->route_offsets[src->route_count] : 0;
            lrsp_column_t* clone = lrsp_column_build(
                result_arena,
                src->column_id,
                src->facility_id,
                src->facility_index,
                src->covered_customers, src->covered_customer_ids,
                src->covered_count,
                src->pairing_cost,
                src->reduced_cost,
                src->total_demand,
                src->total_travel_cost,
                src->route_offsets, src->route_paths, src->route_count,
                src->iteration,
                src->kind);
            (void)total_path;
            res->columns[k] = clone;
        }
        res->column_count = total_cols;
    }

    res->total_runtime = now_seconds() - run_start;

    lrsp_master_destroy(m);
    lrsp_arena_destroy(scratch);

    *out_result = res;
    (void)last_master_objective;
    return LRSP_OK;
}
