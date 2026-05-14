#include "internal.h"

#include <stdlib.h>
#include <string.h>

/*
 * Instance lifecycle and CSR-adjacency construction. The structure is plain
 * heap-resident memory; the arena is reserved for solver scratch only.
 */

static mespprc_status_t ensure_id_capacity(mespprc_instance_t* inst, int external_id) {
    if (external_id < 0) return MESPPRC_ERR_INVALID_ARG;
    if (external_id <= inst->max_external_id) return MESPPRC_OK;

    int new_max = external_id;
    int new_size = new_max + 1;
    int* new_map = (int*)realloc(inst->node_id_to_index, (size_t)new_size * sizeof(int));
    if (!new_map) return MESPPRC_ERR_NOMEM;
    for (int i = inst->max_external_id + 1; i < new_size; ++i) {
        new_map[i] = -1;
    }
    inst->node_id_to_index = new_map;
    inst->max_external_id = new_max;
    return MESPPRC_OK;
}

static mespprc_status_t ensure_arc_capacity(mespprc_instance_t* inst, int needed) {
    if (needed <= inst->arc_capacity) return MESPPRC_OK;
    int new_cap = inst->arc_capacity > 0 ? inst->arc_capacity : 16;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    void* new_arcs = realloc(inst->arcs, (size_t)new_cap * sizeof(mespprc_arc_internal_t));
    if (!new_arcs) return MESPPRC_ERR_NOMEM;
    inst->arcs = (mespprc_arc_internal_t*)new_arcs;

    if (inst->local_dim > 0) {
        void* new_local = realloc(
            inst->arc_local_res,
            (size_t)new_cap * (size_t)inst->local_dim * sizeof(double)
        );
        if (!new_local) return MESPPRC_ERR_NOMEM;
        inst->arc_local_res = (double*)new_local;
    }
    if (inst->global_dim > 0) {
        void* new_global = realloc(
            inst->arc_global_res,
            (size_t)new_cap * (size_t)inst->global_dim * sizeof(double)
        );
        if (!new_global) return MESPPRC_ERR_NOMEM;
        inst->arc_global_res = (double*)new_global;
    }
    inst->arc_capacity = new_cap;
    return MESPPRC_OK;
}

mespprc_status_t mespprc_instance_create(
    int num_nodes,
    int local_dim,
    int global_dim,
    int expected_arc_count,
    mespprc_instance_t** out_instance
) {
    if (!out_instance || num_nodes < 0 || local_dim < 0 || global_dim < 0) {
        return MESPPRC_ERR_INVALID_ARG;
    }

    mespprc_instance_t* inst = (mespprc_instance_t*)calloc(1, sizeof(*inst));
    if (!inst) return MESPPRC_ERR_NOMEM;

    inst->num_nodes = num_nodes;
    inst->local_dim = local_dim;
    inst->global_dim = global_dim;
    inst->source_id = -1;
    inst->sink_id = -1;
    inst->max_external_id = -1;
    inst->finalized = 0;

    if (num_nodes > 0) {
        inst->nodes = (mespprc_node_internal_t*)calloc((size_t)num_nodes,
                                                       sizeof(mespprc_node_internal_t));
        if (!inst->nodes) {
            mespprc_instance_destroy(inst);
            return MESPPRC_ERR_NOMEM;
        }
    }

    if (local_dim > 0) {
        inst->local_limits = (double*)calloc((size_t)local_dim, sizeof(double));
        if (!inst->local_limits) {
            mespprc_instance_destroy(inst);
            return MESPPRC_ERR_NOMEM;
        }
    }
    if (global_dim > 0) {
        inst->global_limits = (double*)calloc((size_t)global_dim, sizeof(double));
        if (!inst->global_limits) {
            mespprc_instance_destroy(inst);
            return MESPPRC_ERR_NOMEM;
        }
    }

    int initial_arc_cap = expected_arc_count > 0 ? expected_arc_count : 16;
    mespprc_status_t s = ensure_arc_capacity(inst, initial_arc_cap);
    if (s != MESPPRC_OK) {
        mespprc_instance_destroy(inst);
        return s;
    }

    *out_instance = inst;
    return MESPPRC_OK;
}

void mespprc_instance_destroy(mespprc_instance_t* instance) {
    if (!instance) return;
    free(instance->nodes);
    free(instance->arcs);
    free(instance->arc_local_res);
    free(instance->arc_global_res);
    free(instance->local_limits);
    free(instance->global_limits);
    free(instance->out_offset);
    free(instance->out_arc_index);
    free(instance->in_offset);
    free(instance->in_arc_index);
    free(instance->node_id_to_index);
    free(instance);
}

mespprc_status_t mespprc_instance_set_local_limits(
    mespprc_instance_t* instance, const double* limits, int dim
) {
    if (!instance || !limits || dim != instance->local_dim) return MESPPRC_ERR_INVALID_ARG;
    if (instance->finalized) return MESPPRC_ERR_INSTANCE_INVALID;
    if (dim > 0) {
        memcpy(instance->local_limits, limits, (size_t)dim * sizeof(double));
    }
    return MESPPRC_OK;
}

mespprc_status_t mespprc_instance_set_global_limits(
    mespprc_instance_t* instance, const double* limits, int dim
) {
    if (!instance || !limits || dim != instance->global_dim) return MESPPRC_ERR_INVALID_ARG;
    if (instance->finalized) return MESPPRC_ERR_INSTANCE_INVALID;
    if (dim > 0) {
        memcpy(instance->global_limits, limits, (size_t)dim * sizeof(double));
    }
    return MESPPRC_OK;
}

mespprc_status_t mespprc_instance_add_node(
    mespprc_instance_t* instance, int node_id, int node_type
) {
    if (!instance || node_id < 0) return MESPPRC_ERR_INVALID_ARG;
    if (instance->finalized) return MESPPRC_ERR_INSTANCE_INVALID;
    if (node_type != MESPPRC_NODE_TYPE_SOURCE
        && node_type != MESPPRC_NODE_TYPE_CUSTOMER
        && node_type != MESPPRC_NODE_TYPE_SINK) {
        return MESPPRC_ERR_INVALID_ARG;
    }
    if (instance->node_count >= instance->num_nodes) {
        return MESPPRC_ERR_INSTANCE_INVALID;  /* over the declared capacity */
    }

    mespprc_status_t s = ensure_id_capacity(instance, node_id);
    if (s != MESPPRC_OK) return s;
    if (instance->node_id_to_index[node_id] != -1) {
        return MESPPRC_ERR_DUPLICATE;
    }

    if (node_type == MESPPRC_NODE_TYPE_SOURCE) {
        if (instance->source_id != -1) return MESPPRC_ERR_DUPLICATE;
        instance->source_id = node_id;
    }
    if (node_type == MESPPRC_NODE_TYPE_SINK) {
        if (instance->sink_id != -1) return MESPPRC_ERR_DUPLICATE;
        instance->sink_id = node_id;
    }

    int dense_index = instance->node_count;
    instance->node_id_to_index[node_id] = dense_index;
    instance->nodes[dense_index].id = node_id;
    instance->nodes[dense_index].type = node_type;
    instance->node_count += 1;
    return MESPPRC_OK;
}

mespprc_status_t mespprc_instance_add_arc(
    mespprc_instance_t* instance,
    int tail, int head, double cost,
    const double* local_res, const double* global_res
) {
    if (!instance) return MESPPRC_ERR_INVALID_ARG;
    if (instance->finalized) return MESPPRC_ERR_INSTANCE_INVALID;
    if (instance->local_dim > 0 && !local_res) return MESPPRC_ERR_INVALID_ARG;
    if (instance->global_dim > 0 && !global_res) return MESPPRC_ERR_INVALID_ARG;

    int tail_idx = mespprc_instance_node_index(instance, tail);
    int head_idx = mespprc_instance_node_index(instance, head);
    if (tail_idx < 0 || head_idx < 0) return MESPPRC_ERR_INVALID_ARG;

    mespprc_status_t s = ensure_arc_capacity(instance, instance->num_arcs + 1);
    if (s != MESPPRC_OK) return s;

    int idx = instance->num_arcs;
    instance->arcs[idx].tail = tail;
    instance->arcs[idx].head = head;
    instance->arcs[idx].cost = cost;
    if (instance->local_dim > 0) {
        memcpy(
            instance->arc_local_res + (size_t)idx * instance->local_dim,
            local_res,
            (size_t)instance->local_dim * sizeof(double)
        );
    }
    if (instance->global_dim > 0) {
        memcpy(
            instance->arc_global_res + (size_t)idx * instance->global_dim,
            global_res,
            (size_t)instance->global_dim * sizeof(double)
        );
    }
    instance->num_arcs += 1;
    return MESPPRC_OK;
}

mespprc_status_t mespprc_instance_finalize(mespprc_instance_t* instance) {
    if (!instance) return MESPPRC_ERR_INVALID_ARG;
    if (instance->finalized) return MESPPRC_OK;
    if (instance->source_id < 0 || instance->sink_id < 0) {
        return MESPPRC_ERR_INSTANCE_INVALID;
    }
    if (instance->node_count != instance->num_nodes) {
        return MESPPRC_ERR_INSTANCE_INVALID;
    }

    int n = instance->num_nodes;
    int m = instance->num_arcs;

    instance->out_offset = (int*)calloc((size_t)n + 1, sizeof(int));
    instance->in_offset = (int*)calloc((size_t)n + 1, sizeof(int));
    if (m > 0) {
        instance->out_arc_index = (int*)calloc((size_t)m, sizeof(int));
        instance->in_arc_index = (int*)calloc((size_t)m, sizeof(int));
    }
    if (!instance->out_offset || !instance->in_offset
        || (m > 0 && (!instance->out_arc_index || !instance->in_arc_index))) {
        return MESPPRC_ERR_NOMEM;
    }

    /* Count out- and in-degree per dense node index. */
    for (int i = 0; i < m; ++i) {
        int t = mespprc_instance_node_index(instance, instance->arcs[i].tail);
        int h = mespprc_instance_node_index(instance, instance->arcs[i].head);
        if (t < 0 || h < 0) return MESPPRC_ERR_INSTANCE_INVALID;
        instance->out_offset[t + 1] += 1;
        instance->in_offset[h + 1] += 1;
    }
    for (int i = 1; i <= n; ++i) {
        instance->out_offset[i] += instance->out_offset[i - 1];
        instance->in_offset[i] += instance->in_offset[i - 1];
    }

    /* Cursor copies for the second pass. */
    int* out_cursor = (int*)calloc((size_t)n, sizeof(int));
    int* in_cursor = (int*)calloc((size_t)n, sizeof(int));
    if (!out_cursor || !in_cursor) {
        free(out_cursor);
        free(in_cursor);
        return MESPPRC_ERR_NOMEM;
    }

    for (int i = 0; i < m; ++i) {
        int t = mespprc_instance_node_index(instance, instance->arcs[i].tail);
        int h = mespprc_instance_node_index(instance, instance->arcs[i].head);
        instance->out_arc_index[instance->out_offset[t] + out_cursor[t]] = i;
        instance->in_arc_index[instance->in_offset[h] + in_cursor[h]] = i;
        out_cursor[t] += 1;
        in_cursor[h] += 1;
    }

    free(out_cursor);
    free(in_cursor);

    instance->finalized = 1;
    return MESPPRC_OK;
}

int mespprc_instance_node_count(const mespprc_instance_t* instance) {
    return instance ? instance->num_nodes : 0;
}
int mespprc_instance_arc_count(const mespprc_instance_t* instance) {
    return instance ? instance->num_arcs : 0;
}
int mespprc_instance_local_dim(const mespprc_instance_t* instance) {
    return instance ? instance->local_dim : 0;
}
int mespprc_instance_global_dim(const mespprc_instance_t* instance) {
    return instance ? instance->global_dim : 0;
}
int mespprc_instance_source_id(const mespprc_instance_t* instance) {
    return instance ? instance->source_id : -1;
}
int mespprc_instance_sink_id(const mespprc_instance_t* instance) {
    return instance ? instance->sink_id : -1;
}
int mespprc_instance_is_finalized(const mespprc_instance_t* instance) {
    return instance ? instance->finalized : 0;
}

mespprc_status_t mespprc_instance_get_arc(
    const mespprc_instance_t* instance,
    int index,
    int* out_tail, int* out_head, double* out_cost,
    double* out_local_res, double* out_global_res
) {
    if (!instance) return MESPPRC_ERR_INVALID_ARG;
    if (index < 0 || index >= instance->num_arcs) return MESPPRC_ERR_INVALID_ARG;
    const mespprc_arc_internal_t* a = &instance->arcs[index];
    if (out_tail) *out_tail = a->tail;
    if (out_head) *out_head = a->head;
    if (out_cost) *out_cost = a->cost;
    if (out_local_res && instance->local_dim > 0) {
        memcpy(out_local_res,
               instance->arc_local_res + (size_t)index * instance->local_dim,
               (size_t)instance->local_dim * sizeof(double));
    }
    if (out_global_res && instance->global_dim > 0) {
        memcpy(out_global_res,
               instance->arc_global_res + (size_t)index * instance->global_dim,
               (size_t)instance->global_dim * sizeof(double));
    }
    return MESPPRC_OK;
}
