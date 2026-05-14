/*
 * Tiny C-side smoke test that exercises the foundation:
 *  1. Library version string is non-empty.
 *  2. Layout self-check returns plausible sizes.
 *  3. An instance can be built, finalized, and destroyed without crashing.
 *
 * Compiled by CMake as `mespprc_csmoke`. Returns non-zero on any failure.
 */

#include "mespprc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(cond, msg)                                         \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "csmoke: FAIL %s (%s:%d)\n",               \
                    (msg), __FILE__, __LINE__);                        \
            return 1;                                                  \
        }                                                              \
    } while (0)

#define CHECK_OK(call, msg)                                            \
    do {                                                               \
        mespprc_status_t _s = (call);                                  \
        if (_s != MESPPRC_OK) {                                        \
            fprintf(stderr,                                            \
                    "csmoke: FAIL %s (status=%d) (%s:%d)\n",           \
                    (msg), (int)_s, __FILE__, __LINE__);               \
            return 1;                                                  \
        }                                                              \
    } while (0)

int main(void) {
    /* 1. Version */
    const char* v = mespprc_version();
    ASSERT_TRUE(v && strlen(v) > 0, "version string non-empty");
    printf("csmoke: mespprc_version=%s\n", v);

    /* 2. Layout self-check */
    mespprc_struct_sizes_t sizes;
    mespprc_struct_sizes(&sizes);
    ASSERT_TRUE(sizes.struct_sizes == sizeof(mespprc_struct_sizes_t),
                "self-reported struct_sizes matches sizeof");
    ASSERT_TRUE(sizes.pointer == sizeof(void*), "pointer size matches");
    ASSERT_TRUE(sizes.double_ == sizeof(double), "double size matches");
    ASSERT_TRUE(sizes.int_ == sizeof(int), "int size matches");
    printf("csmoke: sizes ptr=%llu double=%llu int=%llu\n",
           (unsigned long long)sizes.pointer,
           (unsigned long long)sizes.double_,
           (unsigned long long)sizes.int_);

    /*
     * 3. Build a tiny 3-node instance: source 0, customer 1, sink 2.
     *    Local resources: [route_time, capacity]. Global: [duty_time].
     */
    mespprc_instance_t* inst = NULL;
    CHECK_OK(mespprc_instance_create(3, 2, 1, 4, &inst), "instance_create");

    double local_limits[2] = {100.0, 50.0};
    double global_limits[1] = {200.0};
    CHECK_OK(mespprc_instance_set_local_limits(inst, local_limits, 2),
             "set_local_limits");
    CHECK_OK(mespprc_instance_set_global_limits(inst, global_limits, 1),
             "set_global_limits");

    CHECK_OK(mespprc_instance_add_node(inst, 0, MESPPRC_NODE_TYPE_SOURCE),
             "add_node source");
    CHECK_OK(mespprc_instance_add_node(inst, 1, MESPPRC_NODE_TYPE_CUSTOMER),
             "add_node customer");
    CHECK_OK(mespprc_instance_add_node(inst, 2, MESPPRC_NODE_TYPE_SINK),
             "add_node sink");

    double lr_a[2] = {10.0, 5.0};
    double gr_a[1] = {10.0};
    CHECK_OK(mespprc_instance_add_arc(inst, 0, 1, 5.0, lr_a, gr_a),
             "add_arc 0->1");

    double lr_b[2] = {10.0, 0.0};
    double gr_b[1] = {10.0};
    CHECK_OK(mespprc_instance_add_arc(inst, 1, 2, 5.0, lr_b, gr_b),
             "add_arc 1->2");

    CHECK_OK(mespprc_instance_finalize(inst), "finalize");
    ASSERT_TRUE(mespprc_instance_node_count(inst) == 3, "node count");
    ASSERT_TRUE(mespprc_instance_arc_count(inst) == 2, "arc count");
    ASSERT_TRUE(mespprc_instance_source_id(inst) == 0, "source id");
    ASSERT_TRUE(mespprc_instance_sink_id(inst) == 2, "sink id");
    ASSERT_TRUE(mespprc_instance_is_finalized(inst) == 1, "finalized flag");

    int t = -1, h = -1;
    double cost = -1.0;
    double lr[2] = {0.0, 0.0};
    double gr[1] = {0.0};
    CHECK_OK(mespprc_instance_get_arc(inst, 0, &t, &h, &cost, lr, gr),
             "get_arc 0");
    ASSERT_TRUE(t == 0 && h == 1 && cost == 5.0, "arc 0 round-trip");
    ASSERT_TRUE(lr[0] == 10.0 && lr[1] == 5.0, "arc 0 local_res round-trip");
    ASSERT_TRUE(gr[0] == 10.0, "arc 0 global_res round-trip");

    mespprc_instance_destroy(inst);

    printf("csmoke: OK\n");
    return 0;
}
