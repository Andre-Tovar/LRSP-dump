/*
 * Per-facility reduced-cost graph builder. Mirrors lrsp_solver/pricing_graph.py.
 *
 * Build a `mespprc_instance_t` whose arc costs already encode the dual
 * adjustment, plus enough metadata for the LRSP layer to reconstruct the true
 * (un-discounted) travel cost of any returned route. The MESPPRC instance is
 * fully owned by lrsp_native and freed when the pricing graph is destroyed.
 *
 * Reduced-cost convention (matches Python pricing_graph.py:90-96):
 *   rc(arc into customer i) = base_travel_cost
 *                           - coverage_dual[i]
 *                           - facility_capacity_dual[j] * demand[i]
 *                           - facility_customer_link_dual[(i, j)]   (if linking on)
 *   rc(arc into sink)        = base_travel_cost                     (no discount)
 *
 * Resources:
 *   local : [demand_increment]                    if no time limit
 *           [demand_increment, base_travel_cost]  if time limit
 *   global: []                                    if no time limit
 *           [base_travel_cost]                    if time limit
 *
 * Source node id  = 0
 * Sink   node id  = max(customer_id) + 1
 */

#include "internal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mespprc.h"

#define LRSP_PRICING_SOURCE_NODE_ID 0

typedef struct lrsp_pricing_graph {
    mespprc_instance_t* mespprc_instance;
    int     source_node_id;
    int     sink_node_id;
    int     facility_index;
    /* Storage for the base (un-discounted) arc travel costs, keyed by
     * (tail_external_id, head_external_id). For our pricing graph the only
     * external ids in use are 0 (source), 1..C (customers — NOT sink), and
     * sink_node_id (sink). To keep lookup O(1) we use a 2D dense table sized
     * (sink_node_id+1) × (sink_node_id+1) over arena memory. */
    int     n_total_nodes;          /* sink_node_id + 1 */
    double* base_arc_cost;          /* n_total_nodes * n_total_nodes; -1 == not set */
    /* Pairing-constant offset to add to every column's reduced cost when
     * forming a column from this graph's pricing output. */
    double  pairing_constant;
    /* mapping: customer dense idx -> external id (mirrors `instance.customers`). */
    int*    customer_id_by_dense;
    int     num_customers;
    lrsp_arena_t* arena;
} lrsp_pricing_graph_t;

/* Free a pricing graph and the underlying mespprc instance. */
void lrsp_pricing_graph_destroy(lrsp_pricing_graph_t* g) {
    if (!g) return;
    if (g->mespprc_instance) mespprc_instance_destroy(g->mespprc_instance);
    if (g->arena) lrsp_arena_destroy(g->arena);
}

/* Helper: write the base cost into the 2D table. */
static void set_base_cost(lrsp_pricing_graph_t* g, int tail, int head, double v) {
    g->base_arc_cost[(size_t)tail * (size_t)g->n_total_nodes + (size_t)head] = v;
}

/* Look up the base (un-discounted) travel cost of an arc; returns -1.0 if
 * the arc is not present (which the caller treats as a fatal mismatch). */
double lrsp_pricing_graph_base_arc_cost(
    const lrsp_pricing_graph_t* g, int tail_id, int head_id
) {
    if (!g) return -1.0;
    if (tail_id < 0 || tail_id >= g->n_total_nodes) return -1.0;
    if (head_id < 0 || head_id >= g->n_total_nodes) return -1.0;
    return g->base_arc_cost[(size_t)tail_id * (size_t)g->n_total_nodes
                          + (size_t)head_id];
}

int lrsp_pricing_graph_source(const lrsp_pricing_graph_t* g) {
    return g ? g->source_node_id : -1;
}
int lrsp_pricing_graph_sink(const lrsp_pricing_graph_t* g) {
    return g ? g->sink_node_id : -1;
}
double lrsp_pricing_graph_pairing_constant(const lrsp_pricing_graph_t* g) {
    return g ? g->pairing_constant : 0.0;
}
mespprc_instance_t* lrsp_pricing_graph_mespprc(lrsp_pricing_graph_t* g) {
    return g ? g->mespprc_instance : NULL;
}

double lrsp_pricing_graph_actual_route_travel_cost(
    const lrsp_pricing_graph_t* g, const int* path, int path_len
) {
    if (!g || !path || path_len <= 1) return 0.0;
    double total = 0.0;
    for (int i = 0; i + 1 < path_len; ++i) {
        double c = lrsp_pricing_graph_base_arc_cost(g, path[i], path[i + 1]);
        if (c < 0.0) return NAN;
        total += c;
    }
    return total;
}

/* Public builder.
 *
 * `facility_dense_index` is the dense facility index inside `instance`. The
 * caller passes duals in lrsp_duals_t (coverage indexed by customer dense
 * index, facility_capacity indexed by facility dense index, link indexed
 * row-major as customer*F + facility — currently unused in v1).
 */
lrsp_status_t lrsp_pricing_graph_build(
    const lrsp_instance_t* instance,
    int facility_dense_index,
    const lrsp_duals_t* duals,
    lrsp_pricing_graph_t** out_graph
) {
    if (!instance || !duals || !out_graph) return LRSP_ERR_INVALID_ARG;
    if (facility_dense_index < 0 || facility_dense_index >= instance->num_facilities) {
        return LRSP_ERR_INVALID_ARG;
    }
    *out_graph = NULL;

    int C = instance->num_customers;
    if (C <= 0) return LRSP_ERR_INVALID_ARG;

    lrsp_arena_t* arena = lrsp_arena_create(0);
    if (!arena) return LRSP_ERR_NOMEM;

    lrsp_pricing_graph_t* g = (lrsp_pricing_graph_t*)lrsp_arena_calloc(
        arena, sizeof(*g), sizeof(void*));
    if (!g) { lrsp_arena_destroy(arena); return LRSP_ERR_NOMEM; }
    g->arena = arena;
    g->facility_index = facility_dense_index;
    g->num_customers  = C;

    const lrsp_facility_t* facility = &instance->facilities[facility_dense_index];

    /* Determine sink id = max(customer_id) + 1. The source id is fixed at 0
     * to match the Python convention. Customer ids are arbitrary positive
     * integers; we forbid id 0 from being a customer (which the Akca format
     * naturally satisfies — customer ids start at 1). */
    int max_cust_id = 0;
    for (int c = 0; c < C; ++c) {
        if (instance->customers[c].id > max_cust_id) {
            max_cust_id = instance->customers[c].id;
        }
        if (instance->customers[c].id == LRSP_PRICING_SOURCE_NODE_ID) {
            return LRSP_ERR_INVALID_ARG;
        }
    }
    int source_id = LRSP_PRICING_SOURCE_NODE_ID;
    int sink_id   = max_cust_id + 1;
    g->source_node_id = source_id;
    g->sink_node_id   = sink_id;
    g->n_total_nodes  = sink_id + 1;

    /* Customer dense index → external id, used for emitting node ids in the
     * MESPPRC instance and for cost lookups later. */
    g->customer_id_by_dense = (int*)lrsp_arena_alloc(
        arena, sizeof(int) * (size_t)C, sizeof(int));
    if (!g->customer_id_by_dense) { lrsp_arena_destroy(arena); return LRSP_ERR_NOMEM; }
    for (int c = 0; c < C; ++c) g->customer_id_by_dense[c] = instance->customers[c].id;

    /* Allocate base-arc-cost table; -1 marks "not set". */
    g->base_arc_cost = (double*)lrsp_arena_alloc(
        arena, sizeof(double) * (size_t)g->n_total_nodes * (size_t)g->n_total_nodes,
        sizeof(double));
    if (!g->base_arc_cost) { lrsp_arena_destroy(arena); return LRSP_ERR_NOMEM; }
    for (int i = 0; i < g->n_total_nodes * g->n_total_nodes; ++i) {
        g->base_arc_cost[i] = -1.0;
    }

    /* Build the MESPPRC instance. Resource layout matches pricing_graph.py:
     *   no time limit:  local = [demand],                global = []
     *   with time limit local = [demand, travel_cost],   global = [travel_cost] */
    int has_time_limit = (instance->vehicle_time_limit >= 0.0) ? 1 : 0;
    int local_dim  = has_time_limit ? 2 : 1;
    int global_dim = has_time_limit ? 1 : 0;

    /* Approximate arc count = C (source -> c) + C (c -> sink) + C*(C-1) (c -> c). */
    int est_arc_count = 2 * C + C * (C - 1);

    mespprc_status_t mst;
    mespprc_instance_t* mi = NULL;
    /* Exactly the count we'll add: 1 source + C customers + 1 sink. The
     * mespprc finalize check requires node_count == num_nodes. */
    int declared_node_count = C + 2;
    mst = mespprc_instance_create(
        declared_node_count,
        /*local_dim=*/local_dim, /*global_dim=*/global_dim,
        /*expected_arc_count=*/est_arc_count, &mi);
    if (mst != MESPPRC_OK) { lrsp_arena_destroy(arena); return LRSP_ERR_SOLVER; }
    g->mespprc_instance = mi;

    /* Resource limits. */
    {
        double local_limits[2];
        local_limits[0] = instance->vehicle_capacity;
        if (has_time_limit) local_limits[1] = instance->vehicle_time_limit;
        mst = mespprc_instance_set_local_limits(mi, local_limits, local_dim);
        if (mst != MESPPRC_OK) {
            lrsp_pricing_graph_destroy(g);
            return LRSP_ERR_SOLVER;
        }
    }
    if (has_time_limit) {
        double global_limits[1] = { instance->vehicle_time_limit };
        mst = mespprc_instance_set_global_limits(mi, global_limits, global_dim);
        if (mst != MESPPRC_OK) {
            lrsp_pricing_graph_destroy(g);
            return LRSP_ERR_SOLVER;
        }
    }

    /* Add nodes: source, then customers in dense order, then sink. */
    mespprc_instance_add_node(mi, source_id, MESPPRC_NODE_TYPE_SOURCE);
    for (int c = 0; c < C; ++c) {
        mespprc_instance_add_node(mi, instance->customers[c].id, MESPPRC_NODE_TYPE_CUSTOMER);
    }
    mespprc_instance_add_node(mi, sink_id, MESPPRC_NODE_TYPE_SINK);

    /* Helper closures via local struct of inline math. */
    double cap_dual = 0.0;
    if (duals->facility_capacity && facility_dense_index < duals->num_facilities) {
        cap_dual = duals->facility_capacity[facility_dense_index];
    }

    double facility_x = facility->x;
    double facility_y = facility->y;
    double oper_cost  = instance->vehicle_operating_cost;

    /* Add arcs: source -> customer; customer -> sink; customer -> customer. */
    double local_buf[2];
    double global_buf[1];

    for (int c_head = 0; c_head < C; ++c_head) {
        const lrsp_customer_t* head = &instance->customers[c_head];

        /* Base travel costs at vehicle_operating_cost * Euclidean distance. */
        double base_src   = oper_cost
            * lrsp_euclidean(facility_x, facility_y, head->x, head->y);
        double base_sink  = oper_cost
            * lrsp_euclidean(head->x, head->y, facility_x, facility_y);

        double cov_dual_head = duals->coverage[c_head];
        double link_dual_head = 0.0;
        if (duals->link) {
            link_dual_head = duals->link[(size_t)c_head * (size_t)duals->num_facilities
                                       + (size_t)facility_dense_index];
        }

        /* source -> customer */
        double rc_src = base_src
                      - cov_dual_head
                      - cap_dual * head->demand
                      - link_dual_head;
        local_buf[0] = head->demand;
        if (has_time_limit) {
            local_buf[1] = base_src;
            global_buf[0] = base_src;
        }
        mespprc_instance_add_arc(
            mi, source_id, head->id, rc_src,
            local_buf, has_time_limit ? global_buf : NULL);
        set_base_cost(g, source_id, head->id, base_src);

        /* customer -> sink: NO dual discount (sink is not a customer). */
        local_buf[0] = 0.0;  /* sink contributes no demand */
        if (has_time_limit) {
            local_buf[1] = base_sink;
            global_buf[0] = base_sink;
        }
        mespprc_instance_add_arc(
            mi, head->id, sink_id, base_sink,
            local_buf, has_time_limit ? global_buf : NULL);
        set_base_cost(g, head->id, sink_id, base_sink);
    }

    for (int t = 0; t < C; ++t) {
        const lrsp_customer_t* tail = &instance->customers[t];
        for (int h = 0; h < C; ++h) {
            if (t == h) continue;
            const lrsp_customer_t* head = &instance->customers[h];
            double base = oper_cost
                * lrsp_euclidean(tail->x, tail->y, head->x, head->y);
            double cov_dual_head = duals->coverage[h];
            double link_dual_head = 0.0;
            if (duals->link) {
                link_dual_head = duals->link[(size_t)h * (size_t)duals->num_facilities
                                           + (size_t)facility_dense_index];
            }
            double rc = base - cov_dual_head - cap_dual * head->demand - link_dual_head;
            local_buf[0] = head->demand;
            if (has_time_limit) {
                local_buf[1] = base;
                global_buf[0] = base;
            }
            mespprc_instance_add_arc(
                mi, tail->id, head->id, rc,
                local_buf, has_time_limit ? global_buf : NULL);
            set_base_cost(g, tail->id, head->id, base);
        }
    }

    mst = mespprc_instance_finalize(mi);
    if (mst != MESPPRC_OK) {
        lrsp_pricing_graph_destroy(g);
        return LRSP_ERR_SOLVER;
    }

    /* Pairing constant — matches pricing_graph.py:158. The min_open term is a
     * placeholder in the Python code (multiplied by 0); we keep the same. */
    g->pairing_constant = instance->vehicle_fixed_cost
                        - duals->min_open_facilities * 0.0;

    *out_graph = g;
    return LRSP_OK;
}
