/*
 * CLI runner for the C LRSP solver.
 *
 * Usage:
 *     run_lrsp --instance <path-to-akca-txt> --pricing dp|ip
 *              [--max-iterations N] [--max-cols-per-facility N]
 *              [--no-integer] [--no-linking] [--no-min-open]
 *              [--time-limit-seconds X] [--verbose]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lrsp.h"

static void usage(void) {
    fprintf(stderr,
        "Usage: run_lrsp --instance <path> --pricing dp|ip\n"
        "                 [--max-iterations N] [--max-cols-per-facility N]\n"
        "                 [--no-integer] [--no-linking] [--no-min-open]\n"
        "                 [--time-limit-seconds X] [--verbose]\n"
        "\n"
        "  Linking and min-open lower-bound rows are ON by default (Akca's full\n"
        "  formulation). Pass --no-linking and/or --no-min-open to drop them.\n"
    );
}

int main(int argc, char** argv) {
    const char* instance_path = NULL;
    const char* pricing_arg   = "dp";
    int max_iters = 50;
    int max_cols  = 16;
    int solve_int = 1;
    int use_link  = 1;
    int use_min_open = 1;
    double time_limit = -1.0;
    int verbose = 0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "--instance") == 0 && i + 1 < argc) {
            instance_path = argv[++i];
        } else if (strcmp(a, "--pricing") == 0 && i + 1 < argc) {
            pricing_arg = argv[++i];
        } else if (strcmp(a, "--max-iterations") == 0 && i + 1 < argc) {
            max_iters = atoi(argv[++i]);
        } else if (strcmp(a, "--max-cols-per-facility") == 0 && i + 1 < argc) {
            max_cols = atoi(argv[++i]);
        } else if (strcmp(a, "--no-integer") == 0) {
            solve_int = 0;
        } else if (strcmp(a, "--no-linking") == 0) {
            use_link = 0;
        } else if (strcmp(a, "--no-min-open") == 0) {
            use_min_open = 0;
        } else if (strcmp(a, "--time-limit-seconds") == 0 && i + 1 < argc) {
            time_limit = atof(argv[++i]);
        } else if (strcmp(a, "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "unrecognized argument: %s\n", a);
            usage();
            return 2;
        }
    }
    if (!instance_path) { usage(); return 2; }

    lrsp_pricing_method_t pricing;
    if (strcmp(pricing_arg, "dp") == 0)          pricing = LRSP_PRICING_DP;
    else if (strcmp(pricing_arg, "ip") == 0)     pricing = LRSP_PRICING_IP;
    else if (strcmp(pricing_arg, "hybrid") == 0) pricing = LRSP_PRICING_HYBRID;
    else {
        fprintf(stderr, "--pricing must be 'dp', 'ip', or 'hybrid', got '%s'\n",
                pricing_arg);
        return 2;
    }

    lrsp_instance_t* inst = NULL;
    lrsp_status_t st = lrsp_instance_load_akca_txt(instance_path, 1.0, &inst);
    if (st != LRSP_OK) {
        fprintf(stderr, "load_akca_txt('%s') failed: %s\n",
                instance_path, lrsp_status_name(st));
        return 1;
    }

    lrsp_solver_config_t cfg;
    lrsp_solver_config_default(&cfg);
    cfg.pricing                        = pricing;
    cfg.max_iterations                 = max_iters;
    cfg.max_columns_per_facility       = max_cols;
    cfg.solve_integer_master           = solve_int;
    cfg.use_facility_customer_linking  = use_link;
    cfg.use_min_open_facilities_bound  = use_min_open;
    cfg.time_limit_seconds             = time_limit;
    cfg.verbose                        = verbose;

    printf("instance: %s   customers=%d facilities=%d\n",
           lrsp_instance_name(inst),
           lrsp_instance_num_customers(inst),
           lrsp_instance_num_facilities(inst));
    printf("pricing:  %s   max_iters=%d max_cols/fac=%d  linking=%d min_open=%d\n",
           pricing_arg, max_iters, max_cols, use_link, use_min_open);

    lrsp_result_t* res = NULL;
    st = lrsp_solve(inst, &cfg, &res);
    if (st != LRSP_OK) {
        fprintf(stderr, "lrsp_solve failed: %s\n", lrsp_status_name(st));
        lrsp_instance_destroy(inst);
        return 1;
    }

    /* Aggregate per-kind column counts so the benchmark script can
     * separate seed / Phase 1 route / Phase 2 pairing contributions. */
    int n_total = lrsp_result_column_count(res);
    int n_seed = 0, n_route = 0, n_pairing = 0;
    int max_route_count = 0;
    int total_route_count_sum = 0;  /* sum of routes across all pairing columns */
    for (int i = 0; i < n_total; ++i) {
        int kind = -1;
        if (lrsp_result_column_kind(res, i, &kind) != LRSP_OK) continue;
        if (kind == 0) n_seed++;
        else if (kind == 1) n_route++;
        else if (kind == 2) n_pairing++;
        int rc = lrsp_result_column_route_count(res, i);
        if (rc > max_route_count) max_route_count = rc;
        if (kind == 2) total_route_count_sum += rc;
    }

    printf("\n=== solver result ===\n");
    printf("status:                 %s\n", lrsp_result_status_name(res));
    printf("pricing_engine:         %s\n", lrsp_result_pricing_engine(res));
    printf("iterations:             %d\n", lrsp_result_iteration_count(res));
    printf("pricing_calls:          %d\n", lrsp_result_pricing_call_count(res));
    printf("total_columns:          %d\n", n_total);
    printf("seed_columns:           %d\n", n_seed);
    printf("phase1_route_columns:   %d\n", n_route);
    printf("phase2_pairing_columns: %d\n", n_pairing);
    printf("max_routes_per_column:  %d\n", max_route_count);
    printf("avg_routes_per_pairing: %.2f\n",
           n_pairing > 0 ? (double)total_route_count_sum / (double)n_pairing : 0.0);
    printf("reached_optimality:     %d\n", lrsp_result_reached_optimality(res));
    printf("total_runtime:          %.4fs\n", lrsp_result_total_runtime(res));
    printf("master_runtime:         %.4fs\n", lrsp_result_master_runtime(res));
    printf("pricing_runtime:        %.4fs\n", lrsp_result_pricing_runtime(res));
    if (lrsp_result_has_root_lp(res)) {
        printf("root_lp_objective:      %.6f\n", lrsp_result_root_lp_objective(res));
    } else {
        printf("root_lp_objective:      <not solved>\n");
    }
    if (lrsp_result_has_integer(res)) {
        printf("integer_objective:      %.6f\n", lrsp_result_integer_objective(res));
        int n_open = lrsp_result_open_facility_count(res);
        printf("open_facilities:        %d  ids=", n_open);
        if (n_open > 0) {
            int* ids = (int*)malloc(sizeof(int) * (size_t)n_open);
            if (ids) {
                lrsp_result_open_facility_ids(res, ids, n_open);
                for (int i = 0; i < n_open; ++i) {
                    printf("%d%s", ids[i], i + 1 == n_open ? "\n" : ",");
                }
                free(ids);
            } else { printf("\n"); }
        } else { printf("\n"); }
    } else {
        printf("integer_objective:      <not solved>\n");
    }

    lrsp_result_destroy(res);
    lrsp_instance_destroy(inst);
    return 0;
}
