/*
 * Diagnostic: load p11-f25-v1t1.txt, build master + seeds, solve LP,
 * dump duals; for facility 0 build pricing graph, dump arcs into customers
 * 1..3 (source -> i and 1 -> i); run Phase 1 and dump route count + best cost.
 *
 * This is the file we cross-check against the equivalent Python diagnostic.
 */

#include <stdio.h>

#include "internal.h"
#include "lrsp.h"
#include "mespprc.h"

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : "tests/p11-f25-v1t1.txt";

    lrsp_instance_t* inst = NULL;
    if (lrsp_instance_load_akca_txt(path, 1.0, &inst) != LRSP_OK) {
        fprintf(stderr, "load failed\n"); return 1;
    }

    /* Master + seeds. */
    /* Use the basic formulation here so the dump matches what the v1
     * equivalence test expected (pre-linking). */
    lrsp_master_t* m = lrsp_master_create(inst, /*use_link=*/0,
                                          /*use_min_open=*/0, 0);
    int next_id = lrsp_master_next_column_id(m);
    lrsp_column_t** seeds = NULL; int nseed = 0;
    lrsp_arena_t* master_arena = lrsp_master_arena(m);
    lrsp_build_singleton_warmstart_columns(inst, master_arena, &next_id, &seeds, &nseed);
    int added = lrsp_master_add_columns(m, seeds, nseed);
    printf("seed columns: built=%d added=%d\n", nseed, added);

    lrsp_arena_t* scratch = lrsp_arena_create(0);
    lrsp_master_solution_t* lp = lrsp_master_solve_lp(m, scratch);
    if (!lp || !lp->is_optimal) { fprintf(stderr, "LP not optimal\n"); return 1; }
    printf("LP objective: %.6f\n", lp->objective);

    /* Coverage duals (first 5). */
    int C = lrsp_instance_num_customers(inst);
    int F = lrsp_instance_num_facilities(inst);
    printf("coverage duals (first 5): ");
    for (int i = 0; i < 5 && i < C; ++i) {
        printf("pi[c%d=%d]=%.4f ", i, inst->customers[i].id, lp->duals->coverage[i]);
    }
    printf("\n");
    printf("capacity duals: ");
    for (int j = 0; j < F; ++j) {
        printf("sigma[f%d=%d]=%.4f ", j, inst->facilities[j].id, lp->duals->facility_capacity[j]);
    }
    printf("\n");

    /* Pricing graph for facility 0. */
    lrsp_pricing_graph_t* g = NULL;
    if (lrsp_pricing_graph_build(inst, 0, lp->duals, &g) != LRSP_OK) {
        fprintf(stderr, "pricing_graph_build failed\n"); return 1;
    }
    printf("pricing graph for facility f0=%d (at %.1f, %.1f), source=%d sink=%d, pairing_constant=%.4f\n",
           inst->facilities[0].id, inst->facilities[0].x, inst->facilities[0].y,
           lrsp_pricing_graph_source(g), lrsp_pricing_graph_sink(g),
           lrsp_pricing_graph_pairing_constant(g));

    int src = lrsp_pricing_graph_source(g);
    int snk = lrsp_pricing_graph_sink(g);
    /* Print 5 representative arcs. */
    for (int t = 0; t < 5 && t < C; ++t) {
        int hid = inst->customers[t].id;
        double base_src = lrsp_pricing_graph_base_arc_cost(g, src, hid);
        double base_snk = lrsp_pricing_graph_base_arc_cost(g, hid, snk);
        printf("  arc src->c%d (id=%d): base=%.4f\n", t, hid, base_src);
        printf("  arc c%d->sink: base=%.4f\n", t, base_snk);
    }

    /* Run Phase 1 against the pricing graph's MESPPRC instance. */
    mespprc_instance_t* mi = lrsp_pricing_graph_mespprc(g);
    mespprc_phase1_result_t* p1 = NULL;
    mespprc_status_t mst = mespprc_solve_phase1(mi, 0, &p1);
    if (mst != MESPPRC_OK) {
        fprintf(stderr, "phase1 failed: %d\n", (int)mst); return 1;
    }
    int rc = mespprc_phase1_route_count(p1);
    printf("phase1 returned %d routes\n", rc);

    int neg = 0;
    double best = 0.0;
    for (int i = 0; i < rc; ++i) {
        double c = 0.0;
        if (mespprc_phase1_route_cost(p1, i, &c) != MESPPRC_OK) continue;
        if (c < best) best = c;
        if (c < -1e-6) neg++;
    }
    printf("phase1 negative routes: %d, best (most-negative) cost: %.6f\n", neg, best);

    /* Dump first 3 routes' costs and paths. */
    for (int i = 0; i < rc && i < 3; ++i) {
        double c = 0.0;
        mespprc_phase1_route_cost(p1, i, &c);
        int plen = mespprc_phase1_path_length(p1, i);
        int pbuf[64]; if (plen > 64) plen = 64;
        mespprc_phase1_route_path(p1, i, pbuf, plen);
        printf("  route[%d]: cost=%.4f path=", i, c);
        for (int k = 0; k < plen; ++k) printf("%d%s", pbuf[k], k+1==plen?"":"->");
        printf("\n");
    }

    mespprc_phase1_result_destroy(p1);
    lrsp_pricing_graph_destroy(g);
    lrsp_arena_destroy(scratch);
    lrsp_master_destroy(m);
    lrsp_instance_destroy(inst);
    return 0;
}
