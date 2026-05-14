/*
 * compare_ip_dp — run the C LRSP solver with each pricing engine on the same
 * instance and write a CSV row capturing objectives and timings.
 *
 * Usage:
 *     compare_ip_dp --instance <path> [--out <csv-path>]
 *                   [--max-iterations N] [--max-cols-per-facility N]
 *                   [--time-limit-seconds X]
 *
 * Default --out is results/c_lrsp_comparison/raw_results.csv (appended to,
 * with a header written if the file is empty / new).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lrsp.h"

static int run_one(
    const lrsp_instance_t* inst,
    const char* engine_name,
    lrsp_pricing_method_t pricing,
    int max_iters, int max_cols, double time_limit,
    int* out_iters, int* out_cols,
    double* out_runtime, double* out_master, double* out_pricing,
    double* out_root_lp, double* out_integer, int* out_open
) {
    lrsp_solver_config_t cfg;
    lrsp_solver_config_default(&cfg);
    cfg.pricing                  = pricing;
    cfg.max_iterations           = max_iters;
    cfg.max_columns_per_facility = max_cols;
    cfg.time_limit_seconds       = time_limit;

    lrsp_result_t* res = NULL;
    lrsp_status_t st = lrsp_solve(inst, &cfg, &res);
    if (st != LRSP_OK || !res) {
        fprintf(stderr, "lrsp_solve failed for %s: %s\n",
                engine_name, lrsp_status_name(st));
        return 1;
    }
    *out_iters   = lrsp_result_iteration_count(res);
    *out_cols    = lrsp_result_column_count(res);
    *out_runtime = lrsp_result_total_runtime(res);
    *out_master  = lrsp_result_master_runtime(res);
    *out_pricing = lrsp_result_pricing_runtime(res);
    *out_root_lp = lrsp_result_has_root_lp(res)
        ? lrsp_result_root_lp_objective(res) : 0.0/1.0;
    *out_integer = lrsp_result_has_integer(res)
        ? lrsp_result_integer_objective(res) : 0.0/1.0;
    *out_open    = lrsp_result_open_facility_count(res);

    printf("[%s] iters=%d cols=%d total=%.3fs root_lp=%.6f integer=%.6f open=%d\n",
           engine_name, *out_iters, *out_cols, *out_runtime,
           *out_root_lp, *out_integer, *out_open);

    lrsp_result_destroy(res);
    return 0;
}

int main(int argc, char** argv) {
    const char* instance_path = NULL;
    const char* out_path = "results/c_lrsp_comparison/raw_results.csv";
    int max_iters = 50;
    int max_cols  = 16;
    double time_limit = -1.0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "--instance") == 0 && i + 1 < argc) {
            instance_path = argv[++i];
        } else if (strcmp(a, "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(a, "--max-iterations") == 0 && i + 1 < argc) {
            max_iters = atoi(argv[++i]);
        } else if (strcmp(a, "--max-cols-per-facility") == 0 && i + 1 < argc) {
            max_cols = atoi(argv[++i]);
        } else if (strcmp(a, "--time-limit-seconds") == 0 && i + 1 < argc) {
            time_limit = atof(argv[++i]);
        } else {
            fprintf(stderr, "unrecognized argument: %s\n", a);
            return 2;
        }
    }
    if (!instance_path) {
        fprintf(stderr,
            "Usage: compare_ip_dp --instance <path> [--out <csv>]\n"
            "                     [--max-iterations N] [--max-cols-per-facility N]\n"
            "                     [--time-limit-seconds X]\n");
        return 2;
    }

    lrsp_instance_t* inst = NULL;
    if (lrsp_instance_load_akca_txt(instance_path, 1.0, &inst) != LRSP_OK) {
        fprintf(stderr, "load_akca_txt failed for '%s'\n", instance_path);
        return 1;
    }
    const char* name = lrsp_instance_name(inst);
    printf("instance: %s   customers=%d facilities=%d\n",
           name, lrsp_instance_num_customers(inst), lrsp_instance_num_facilities(inst));

    /* Run DP. */
    int iters_dp, cols_dp, open_dp;
    double rt_dp, mt_dp, pt_dp, lp_dp, ip_dp;
    if (run_one(inst, "DP", LRSP_PRICING_DP, max_iters, max_cols, time_limit,
                &iters_dp, &cols_dp, &rt_dp, &mt_dp, &pt_dp, &lp_dp, &ip_dp, &open_dp)
        != 0) { lrsp_instance_destroy(inst); return 1; }

    /* Run IP. */
    int iters_ip, cols_ip, open_ip;
    double rt_ip, mt_ip, pt_ip, lp_ip, ip_ip;
    if (run_one(inst, "IP", LRSP_PRICING_IP, max_iters, max_cols, time_limit,
                &iters_ip, &cols_ip, &rt_ip, &mt_ip, &pt_ip, &lp_ip, &ip_ip, &open_ip)
        != 0) { lrsp_instance_destroy(inst); return 1; }

    /* Append to CSV. */
    FILE* f = fopen(out_path, "ab+");
    if (!f) {
        fprintf(stderr, "could not open %s for write\n", out_path);
        lrsp_instance_destroy(inst);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size == 0) {
        fputs("instance,pricing,iterations,columns,total_seconds,master_seconds,"
              "pricing_seconds,root_lp_objective,integer_objective,"
              "open_facilities\n", f);
    }
    fprintf(f, "%s,DP,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n",
        name, iters_dp, cols_dp, rt_dp, mt_dp, pt_dp, lp_dp, ip_dp, open_dp);
    fprintf(f, "%s,IP,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n",
        name, iters_ip, cols_ip, rt_ip, mt_ip, pt_ip, lp_ip, ip_ip, open_ip);
    fclose(f);
    printf("appended to %s\n", out_path);

    /* Print a one-line summary contrast. */
    printf("\n=== summary ===\n");
    printf("DP iters=%d cols=%d  IP iters=%d cols=%d\n",
           iters_dp, cols_dp, iters_ip, cols_ip);
    printf("DP root_lp=%.6f  IP root_lp=%.6f  diff=%.2e\n",
           lp_dp, lp_ip, lp_dp - lp_ip);
    printf("DP integer=%.6f  IP integer=%.6f  diff=%.2e\n",
           ip_dp, ip_ip, ip_dp - ip_ip);
    printf("DP total=%.3fs  IP total=%.3fs  speedup IP/DP=%.2fx\n",
           rt_dp, rt_ip, rt_dp / (rt_ip > 0.0 ? rt_ip : 1e-9));

    lrsp_instance_destroy(inst);
    return 0;
}
