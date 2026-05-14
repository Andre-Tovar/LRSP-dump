/*
 * Round-trip test for the Akca .txt loader. Loads a known instance, asserts
 * counts and a few field values match what we read out of the file by eye.
 *
 * The instance: Akca p11-f25-v1t1.txt (5 facilities, 25 customers,
 * vehicle_capacity 150, facility_capacity 500, vehicle_time_limit 240,
 * vehicle_fixed_cost 225). First customer is id=1 at (-99,-97) with demand=6.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lrsp.h"

static int fail = 0;
#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); fail = 1; } \
} while (0)

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] :
        "../Akca Repo/routingproblems-lrspcode-39e47f81716c/comb_pricing_pro-6/p11-f25-v1t1.txt";

    lrsp_instance_t* inst = NULL;
    lrsp_status_t st = lrsp_instance_load_akca_txt(path, /*operating_cost=*/1.0, &inst);
    if (st != LRSP_OK) {
        fprintf(stderr, "load_akca_txt(%s) failed: %s\n",
                path, lrsp_status_name(st));
        return 1;
    }

    printf("loaded: %s (customers=%d facilities=%d)\n",
           lrsp_instance_name(inst),
           lrsp_instance_num_customers(inst),
           lrsp_instance_num_facilities(inst));

    ASSERT(lrsp_instance_num_facilities(inst) == 5, "5 facilities");
    ASSERT(lrsp_instance_num_customers(inst) == 25, "25 customers");
    ASSERT(lrsp_instance_vehicle_capacity(inst) == 150.0, "vehicle capacity 150");
    ASSERT(lrsp_instance_vehicle_fixed_cost(inst) == 225.0, "fixed cost 225");
    ASSERT(lrsp_instance_vehicle_time_limit(inst) == 240.0, "time limit 240");
    ASSERT(lrsp_instance_is_finalized(inst) == 1, "finalized");

    int    cid = -1; double cx, cy, cd;
    st = lrsp_instance_get_customer(inst, 0, &cid, &cx, &cy, &cd);
    ASSERT(st == LRSP_OK, "get_customer 0");
    ASSERT(cid == 1, "first customer id == 1");
    ASSERT(cx == -99.0, "first customer x == -99");
    ASSERT(cy == -97.0, "first customer y == -97");
    ASSERT(cd == 6.0, "first customer demand == 6");

    int    fid = -1; double fx, fy, fcost, fcap;
    st = lrsp_instance_get_facility(inst, 0, &fid, &fx, &fy, &fcost, &fcap);
    ASSERT(st == LRSP_OK, "get_facility 0");
    /* The first facility row in the file is id=26 (right after 25 customers). */
    ASSERT(fid == 26, "first facility id == 26");
    ASSERT(fcap == 500.0, "first facility capacity == 500");
    /* Opening cost for facility 0 is 1615 from line 2. */
    ASSERT(fcost == 1615.0, "first facility opening cost == 1615");

    lrsp_instance_destroy(inst);

    /* Now confirm a missing-time-limit case is rejected. We synthesise a
     * minimal LRP-style file (line 4 has only 2 numbers) and assert load
     * returns LRSP_ERR_PARSE. */
    {
        const char* tmp = "tmp_lrp_no_time.txt";
        FILE* f = fopen(tmp, "wb");
        if (!f) {
            fprintf(stderr, "could not write %s\n", tmp);
            return fail ? 1 : 1;
        }
        fputs("1 1\n10\n0 1\n50 200\n1 0 0 0 5\n2 1 1 0 0\n", f);
        fclose(f);

        lrsp_instance_t* bad = NULL;
        st = lrsp_instance_load_akca_txt(tmp, 1.0, &bad);
        ASSERT(st == LRSP_ERR_PARSE,
               "LRP-style file (no vehicle_time_limit) is rejected");
        ASSERT(bad == NULL, "no instance handle on parse error");
        remove(tmp);
    }

    if (fail) {
        fprintf(stderr, "test_instance_io: at least one assertion failed\n");
        return 1;
    }
    printf("test_instance_io: ok\n");
    return 0;
}
