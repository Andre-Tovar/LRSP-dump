/*
 * Tiny smoke test that confirms the lrsp_native library is loadable, the
 * version string is set, and the layout self-check struct is wired through.
 * Used during foundation work.
 */

#include <stdio.h>
#include <string.h>

#include "lrsp.h"

static int fail = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); fail = 1; } \
} while (0)

int main(void) {
    /* version */
    const char* v = lrsp_version();
    ASSERT(v && strlen(v) > 0, "lrsp_version() should return a non-empty string");
    printf("version: %s\n", v ? v : "(null)");

    /* status names */
    ASSERT(strcmp(lrsp_status_name(LRSP_OK), "OK") == 0, "OK name");
    ASSERT(strcmp(lrsp_status_name(LRSP_ERR_NOMEM), "NOMEM") == 0, "NOMEM name");

    /* layout */
    lrsp_struct_sizes_t sizes;
    memset(&sizes, 0, sizeof(sizes));
    lrsp_struct_sizes(&sizes);
    ASSERT(sizes.struct_sizes == sizeof(lrsp_struct_sizes_t),
           "struct_sizes self-report");
    ASSERT(sizes.pointer == sizeof(void*), "pointer size matches");
    ASSERT(sizes.double_ == sizeof(double), "double size matches");
    printf("sizes: ptr=%llu double=%llu int=%llu status=%llu\n",
           (unsigned long long)sizes.pointer,
           (unsigned long long)sizes.double_,
           (unsigned long long)sizes.int_,
           (unsigned long long)sizes.status_t);

    /* solver config defaults */
    lrsp_solver_config_t cfg;
    lrsp_solver_config_default(&cfg);
    ASSERT(cfg.pricing == LRSP_PRICING_DP, "default pricing is DP");
    ASSERT(cfg.max_iterations > 0, "default max_iterations > 0");
    ASSERT(cfg.time_limit_seconds < 0, "default time_limit < 0 means no limit");

    /* instance lifecycle */
    lrsp_instance_t* inst = NULL;
    lrsp_status_t st = lrsp_instance_create(
        /*num_customers=*/3, /*num_facilities=*/2,
        /*vehicle_capacity=*/100.0, /*vehicle_fixed_cost=*/10.0,
        /*vehicle_operating_cost=*/1.0, /*vehicle_time_limit=*/120.0,
        &inst);
    ASSERT(st == LRSP_OK && inst != NULL, "lrsp_instance_create");
    if (inst) {
        ASSERT(lrsp_instance_add_customer(inst, 1, 10.0, 20.0, 15.0) == LRSP_OK,
               "add customer 1");
        ASSERT(lrsp_instance_add_customer(inst, 2, 30.0, 40.0, 25.0) == LRSP_OK,
               "add customer 2");
        ASSERT(lrsp_instance_add_customer(inst, 3, 50.0, 60.0, 35.0) == LRSP_OK,
               "add customer 3");
        ASSERT(lrsp_instance_add_facility(inst, 4, 0.0, 0.0, 100.0, 200.0) == LRSP_OK,
               "add facility 4");
        ASSERT(lrsp_instance_add_facility(inst, 5, 70.0, 70.0, 120.0, 200.0) == LRSP_OK,
               "add facility 5");
        ASSERT(lrsp_instance_finalize(inst) == LRSP_OK, "finalize");

        ASSERT(lrsp_instance_num_customers(inst) == 3, "num_customers");
        ASSERT(lrsp_instance_num_facilities(inst) == 2, "num_facilities");
        ASSERT(lrsp_instance_vehicle_capacity(inst) == 100.0, "capacity");
        ASSERT(lrsp_instance_vehicle_time_limit(inst) == 120.0, "time limit");
        ASSERT(lrsp_instance_is_finalized(inst) == 1, "finalized flag");

        int cid = -1; double cx = 0, cy = 0, cd = 0;
        st = lrsp_instance_get_customer(inst, 1, &cid, &cx, &cy, &cd);
        ASSERT(st == LRSP_OK && cid == 2 && cx == 30.0 && cd == 25.0,
               "get customer at index 1");

        lrsp_instance_destroy(inst);
    }

    if (fail) {
        fprintf(stderr, "csmoke: at least one assertion failed\n");
        return 1;
    }
    printf("csmoke: ok\n");
    return 0;
}
