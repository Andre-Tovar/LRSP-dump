/*
 * LRSP restricted master problem, backed by HiGHS.
 *
 * Mirrors `lrsp_solver/master_problem.py:101-225`. The Akca set-partitioning
 * formulation in full:
 *
 *   variables
 *       y_j  for each facility j   ∈ [0,1]   (continuous in LP, binary in IP)
 *       λ_p  for each column p     ∈ [0,1]
 *
 *   objective
 *       min  Σ_j  opening_cost_j * y_j  +  Σ_p  pairing_cost_p * λ_p
 *
 *   rows
 *       coverage_i      :  Σ_{p covers i} λ_p                         == 1
 *       capacity_j      :  Σ_{p facility=j} d_p · λ_p − Cap_j · y_j   ≤ 0
 *       link_{i,j}      :  Σ_{p facility=j ∧ i∈p} λ_p − y_j           ≤ 0
 *       min_open        :  Σ_j  y_j                                    ≥ K
 *
 *   K = `instance.minimum_required_open_facilities()` is the greedy lower
 *   bound from packing the largest facility capacities until total demand is
 *   covered.
 *
 * Linking and min-open rows are gated by configuration flags (default ON,
 * matching the Python solver and Akca's dissertation formulation).
 *
 * HiGHS column / row ordering:
 *   columns: 0..F-1                 facility-open vars y_j
 *            F..F+K-1               pool columns λ_p (in insertion order)
 *   rows:    0..C-1                 coverage rows (== 1)
 *            C..C+F-1               capacity rows (≤ 0)
 *            C+F..C+F+C*F-1         linking rows (≤ 0)             [if enabled]
 *            C+F+C*F                min-open row (≥ K)              [if enabled]
 */

#include "internal.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "highs_c_api.h"

#define HUGE_NEG (-1.0e30)

typedef struct lrsp_master {
    void* highs;

    const lrsp_instance_t* instance;
    int num_customers;
    int num_facilities;

    /* Customer external id -> dense index. Sized at create. */
    int   max_customer_id;
    int*  customer_id_to_index;

    /* Facility external id -> dense index. */
    int   max_facility_id;
    int*  facility_id_to_index;

    /* Optional row families — captured at create() time so add_columns and
     * solve_lp can reference them without re-reading config. */
    int   use_link;
    int   use_min_open;
    int   link_row_offset;            /* C + F when use_link, else -1 */
    int   min_open_row_index;         /* C + F + (use_link ? C*F : 0), else -1 */
    int   min_open_K;                 /* lower bound on # open facilities */
    int   total_rows;                 /* cached so we don't ask HiGHS each call */

    /* Column pool — singly linked list inside master->arena, plus a parallel
     * array for index lookup. */
    lrsp_arena_t* arena;
    lrsp_column_t** columns;          /* dynamic array */
    int columns_count;
    int columns_capacity;
    lrsp_column_t* pool_head;         /* singly linked list head for dedup */
    int next_column_id;
} lrsp_master_t;

/* Greedy lower bound on the number of facilities that must be open: pack the
 * largest capacities until total demand is covered. Mirrors
 * `LRSPInstance.minimum_required_open_facilities()` (lrsp_solver/instance.py). */
static int compute_min_open_K(const lrsp_instance_t* inst) {
    double total_demand = 0.0;
    for (int i = 0; i < inst->num_customers; ++i) {
        total_demand += inst->customers[i].demand;
    }
    if (total_demand <= 0.0) return 0;

    int F = inst->num_facilities;
    double* caps = (double*)malloc(sizeof(double) * (size_t)F);
    if (!caps) return 1; /* on alloc failure, fall back to "at least one" */
    for (int j = 0; j < F; ++j) caps[j] = inst->facilities[j].capacity;
    /* Sort descending (insertion sort — F is small). */
    for (int i = 1; i < F; ++i) {
        double cur = caps[i];
        int j = i - 1;
        while (j >= 0 && caps[j] < cur) { caps[j+1] = caps[j]; j--; }
        caps[j+1] = cur;
    }
    double running = 0.0;
    int K = F;
    for (int count = 1; count <= F; ++count) {
        running += caps[count - 1];
        if (running + 1e-9 >= total_demand) { K = count; break; }
    }
    free(caps);
    return K;
}

/* ---------- Construction ---------- */

static int build_id_lookup(int* out_max_id, int** out_lookup,
                           const int* ids, int n) {
    int max_id = 0;
    for (int i = 0; i < n; ++i) if (ids[i] > max_id) max_id = ids[i];
    *out_max_id = max_id;
    *out_lookup = (int*)malloc(sizeof(int) * (size_t)(max_id + 1));
    if (!*out_lookup) return -1;
    for (int i = 0; i <= max_id; ++i) (*out_lookup)[i] = -1;
    for (int i = 0; i < n; ++i) (*out_lookup)[ids[i]] = i;
    return 0;
}

lrsp_master_t* lrsp_master_create(
    const lrsp_instance_t* instance,
    int use_link,
    int use_min_open,
    int verbose
) {
    if (!instance) return NULL;
    int C = instance->num_customers;
    int F = instance->num_facilities;
    if (C <= 0 || F <= 0) return NULL;

    lrsp_master_t* m = (lrsp_master_t*)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->instance       = instance;
    m->num_customers  = C;
    m->num_facilities = F;
    m->use_link       = use_link ? 1 : 0;
    m->use_min_open   = use_min_open ? 1 : 0;
    m->link_row_offset    = m->use_link     ? (C + F) : -1;
    m->min_open_row_index = m->use_min_open
        ? (C + F + (m->use_link ? C * F : 0)) : -1;
    m->min_open_K     = m->use_min_open ? compute_min_open_K(instance) : 0;
    m->total_rows     = C + F
                      + (m->use_link ? C * F : 0)
                      + (m->use_min_open ? 1 : 0);
    m->arena          = lrsp_arena_create(0);
    m->columns_capacity = 0;
    m->columns_count    = 0;
    m->columns          = NULL;
    m->pool_head        = NULL;
    m->next_column_id   = 1;
    if (!m->arena) { free(m); return NULL; }

    /* customer id <-> dense index */
    int* cust_ids = (int*)malloc(sizeof(int) * (size_t)C);
    int* fac_ids  = (int*)malloc(sizeof(int) * (size_t)F);
    if (!cust_ids || !fac_ids) {
        free(cust_ids); free(fac_ids);
        lrsp_arena_destroy(m->arena); free(m);
        return NULL;
    }
    for (int i = 0; i < C; ++i) cust_ids[i] = instance->customers[i].id;
    for (int j = 0; j < F; ++j) fac_ids[j]  = instance->facilities[j].id;
    if (build_id_lookup(&m->max_customer_id, &m->customer_id_to_index,
                        cust_ids, C) != 0
        || build_id_lookup(&m->max_facility_id, &m->facility_id_to_index,
                           fac_ids, F) != 0) {
        free(cust_ids); free(fac_ids);
        lrsp_arena_destroy(m->arena); free(m);
        return NULL;
    }
    free(cust_ids); free(fac_ids);

    /* Build HiGHS instance. */
    m->highs = Highs_create();
    if (!m->highs) {
        free(m->customer_id_to_index); free(m->facility_id_to_index);
        lrsp_arena_destroy(m->arena); free(m);
        return NULL;
    }
    Highs_setBoolOptionValue(m->highs, "output_flag", verbose ? 1 : 0);
    Highs_changeObjectiveSense(m->highs, kHighsObjSenseMinimize);

    /* 1) Coverage rows (== 1), empty matrix — to be filled by addCol. */
    for (int i = 0; i < C; ++i) {
        Highs_addRow(m->highs, 1.0, 1.0, 0, NULL, NULL);
    }
    /* 2) Capacity rows (≤ 0), empty matrix. */
    for (int j = 0; j < F; ++j) {
        Highs_addRow(m->highs, HUGE_NEG, 0.0, 0, NULL, NULL);
    }
    /* 3) Linking rows (≤ 0), empty matrix — one per (customer, facility). */
    if (m->use_link) {
        for (int t = 0; t < C * F; ++t) {
            Highs_addRow(m->highs, HUGE_NEG, 0.0, 0, NULL, NULL);
        }
    }

    /* 4) Facility-open columns y_j. Each touches:
     *      capacity row j           with coefficient -Cap_j
     *      linking rows (i, j)      with coefficient -1, for every customer i */
    int y_nz_count = 1 + (m->use_link ? C : 0);
    int*    y_idx = (int*)malloc(sizeof(int) * (size_t)y_nz_count);
    double* y_val = (double*)malloc(sizeof(double) * (size_t)y_nz_count);
    if (!y_idx || !y_val) {
        free(y_idx); free(y_val);
        Highs_destroy(m->highs);
        free(m->customer_id_to_index); free(m->facility_id_to_index);
        lrsp_arena_destroy(m->arena); free(m);
        return NULL;
    }
    for (int j = 0; j < F; ++j) {
        const lrsp_facility_t* f = &instance->facilities[j];
        int p = 0;
        y_idx[p] = C + j; y_val[p] = -f->capacity; p++;
        if (m->use_link) {
            for (int i = 0; i < C; ++i) {
                y_idx[p] = m->link_row_offset + i * F + j;
                y_val[p] = -1.0;
                p++;
            }
        }
        Highs_addCol(m->highs, f->opening_cost, 0.0, 1.0, p, y_idx, y_val);
    }
    free(y_idx); free(y_val);

    /* 5) Min-open row, last. We add it AFTER y_j cols exist so the addRow
     * call can specify each y_j's coefficient (+1) directly. */
    if (m->use_min_open && m->min_open_K > 0) {
        int*    mo_idx = (int*)malloc(sizeof(int) * (size_t)F);
        double* mo_val = (double*)malloc(sizeof(double) * (size_t)F);
        if (!mo_idx || !mo_val) {
            free(mo_idx); free(mo_val);
            Highs_destroy(m->highs);
            free(m->customer_id_to_index); free(m->facility_id_to_index);
            lrsp_arena_destroy(m->arena); free(m);
            return NULL;
        }
        for (int j = 0; j < F; ++j) { mo_idx[j] = j; mo_val[j] = 1.0; }
        Highs_addRow(m->highs, /*lower=*/(double)m->min_open_K, /*upper=*/HUGE_VAL,
                     F, mo_idx, mo_val);
        free(mo_idx); free(mo_val);
    } else {
        /* If K == 0 we skip the row — it is trivially satisfied and adding
         * a vacuous row would still surface as a dual. */
        m->min_open_row_index = -1;
        m->use_min_open = 0;
        m->total_rows = C + F + (m->use_link ? C * F : 0);
    }

    return m;
}

void lrsp_master_destroy(lrsp_master_t* m) {
    if (!m) return;
    if (m->highs) Highs_destroy(m->highs);
    free(m->columns);
    free(m->customer_id_to_index);
    free(m->facility_id_to_index);
    if (m->arena) lrsp_arena_destroy(m->arena);
    free(m);
}

int lrsp_master_column_count(const lrsp_master_t* m) {
    return m ? m->columns_count : 0;
}

lrsp_arena_t* lrsp_master_arena(lrsp_master_t* m) {
    return m ? m->arena : NULL;
}

int lrsp_master_next_column_id(lrsp_master_t* m) {
    return m ? m->next_column_id++ : 0;
}

const lrsp_column_t* lrsp_master_column(const lrsp_master_t* m, int idx) {
    if (!m || idx < 0 || idx >= m->columns_count) return NULL;
    return m->columns[idx];
}

/* Add columns to the master, returning the number actually inserted (after
 * dedup). Each accepted column is appended both to m->columns[] and to the
 * pool linked list. The HiGHS handle gets one Highs_addCol per insertion. */
int lrsp_master_add_columns(lrsp_master_t* m, lrsp_column_t** cols, int n) {
    if (!m || !cols || n <= 0) return 0;

    /* Pre-allocate column storage. */
    int needed = m->columns_count + n;
    if (needed > m->columns_capacity) {
        int new_cap = m->columns_capacity == 0 ? 8 : m->columns_capacity * 2;
        while (new_cap < needed) new_cap *= 2;
        lrsp_column_t** p = (lrsp_column_t**)realloc(
            m->columns, (size_t)new_cap * sizeof(*p));
        if (!p) return 0;
        m->columns = p;
        m->columns_capacity = new_cap;
    }

    int added = 0;
    int C = m->num_customers;
    int F = m->num_facilities;

    for (int k = 0; k < n; ++k) {
        lrsp_column_t* col = cols[k];
        if (!col) continue;
        if (lrsp_column_pool_contains(m->pool_head, col->signature)) continue;

        if (col->column_id <= 0) col->column_id = m->next_column_id;
        if (col->column_id >= m->next_column_id) m->next_column_id = col->column_id + 1;

        /* Build the HiGHS sparse column.
         *   coverage rows: covered_count entries (+1 each)
         *   capacity row:  1 entry (+demand_total)
         *   linking rows:  covered_count entries (+1 each), if use_link
         */
        int link_nz = m->use_link ? col->covered_count : 0;
        int nz_count = col->covered_count + 1 + link_nz;
        int*   indices = (int*)malloc(sizeof(int) * (size_t)nz_count);
        double* values = (double*)malloc(sizeof(double) * (size_t)nz_count);
        if (!indices || !values) {
            free(indices); free(values);
            return added;
        }
        int p = 0;
        for (int t = 0; t < col->covered_count; ++t) {
            int dense = col->covered_customers[t];
            indices[p] = dense;       /* coverage row index = customer dense idx */
            values[p]  = 1.0;
            p++;
        }
        indices[p] = C + col->facility_index;
        values[p]  = col->total_demand;
        p++;
        if (m->use_link) {
            for (int t = 0; t < col->covered_count; ++t) {
                int dense = col->covered_customers[t];
                indices[p] = m->link_row_offset + dense * F + col->facility_index;
                values[p]  = 1.0;
                p++;
            }
        }

        Highs_addCol(m->highs,
                     /*cost=*/col->pairing_cost,
                     /*lower=*/0.0, /*upper=*/1.0,
                     /*num_new_nz=*/nz_count,
                     indices, values);
        free(indices); free(values);

        /* Append to pool. */
        col->next = m->pool_head;
        m->pool_head = col;
        m->columns[m->columns_count++] = col;
        added++;
    }
    return added;
}

/* ---------- Solving ---------- */

static void set_all_integrality(void* highs, int num_cols, int variable_type) {
    if (num_cols <= 0) return;
    int* mask = (int*)malloc(sizeof(int) * (size_t)num_cols);
    int* vals = (int*)malloc(sizeof(int) * (size_t)num_cols);
    if (!mask || !vals) { free(mask); free(vals); return; }
    for (int i = 0; i < num_cols; ++i) { mask[i] = i; vals[i] = variable_type; }
    Highs_changeColsIntegralityBySet(highs, num_cols, mask, vals);
    free(mask); free(vals);
}

static lrsp_master_solution_t* alloc_solution(
    lrsp_arena_t* arena, int num_facilities, int num_columns
) {
    lrsp_master_solution_t* s = (lrsp_master_solution_t*)lrsp_arena_calloc(
        arena, sizeof(*s), sizeof(void*));
    if (!s) return NULL;
    s->facility_open_values = (double*)lrsp_arena_calloc(
        arena, sizeof(double) * (size_t)(num_facilities > 0 ? num_facilities : 1),
        sizeof(double));
    s->column_values = (double*)lrsp_arena_calloc(
        arena, sizeof(double) * (size_t)(num_columns > 0 ? num_columns : 1),
        sizeof(double));
    s->selected_column_indices = (int*)lrsp_arena_calloc(
        arena, sizeof(int) * (size_t)(num_columns > 0 ? num_columns : 1),
        sizeof(int));
    return s;
}

/* Solve as LP (continuous on every column) and populate duals. */
lrsp_master_solution_t* lrsp_master_solve_lp(
    lrsp_master_t* m, lrsp_arena_t* result_arena
) {
    if (!m || !result_arena) return NULL;
    int F = m->num_facilities;
    int C = m->num_customers;
    int total_cols = F + m->columns_count;
    int total_rows = m->total_rows;

    set_all_integrality(m->highs, total_cols, kHighsVarTypeContinuous);

    HighsInt rc = Highs_run(m->highs);
    (void)rc;

    lrsp_master_solution_t* sol =
        alloc_solution(result_arena, F, m->columns_count);
    if (!sol) return NULL;
    sol->model_status = (int)Highs_getModelStatus(m->highs);
    sol->is_optimal   = (sol->model_status == kHighsModelStatusOptimal);

    if (!sol->is_optimal) {
        sol->objective = NAN;
        return sol;
    }
    sol->objective = Highs_getObjectiveValue(m->highs);

    double* col_value = (double*)malloc(sizeof(double) * (size_t)total_cols);
    double* row_dual  = (double*)malloc(sizeof(double) * (size_t)total_rows);
    if (!col_value || !row_dual) {
        free(col_value); free(row_dual);
        return sol;
    }
    Highs_getSolution(m->highs, col_value, NULL, NULL, row_dual);

    /* Facility open values. */
    for (int j = 0; j < F; ++j) sol->facility_open_values[j] = col_value[j];

    /* Pool column values. */
    for (int k = 0; k < m->columns_count; ++k) {
        sol->column_values[k] = col_value[F + k];
        if (col_value[F + k] > 1.0e-6) {
            sol->selected_column_indices[sol->selected_column_count++] = k;
        }
    }

    /* Duals. */
    sol->duals = lrsp_duals_create(result_arena, C, F, m->use_link);
    if (sol->duals) {
        for (int i = 0; i < C; ++i) sol->duals->coverage[i]          = row_dual[i];
        for (int j = 0; j < F; ++j) sol->duals->facility_capacity[j] = row_dual[C + j];
        if (m->use_link && sol->duals->link) {
            int off = m->link_row_offset;
            for (int i = 0; i < C; ++i) {
                for (int j = 0; j < F; ++j) {
                    sol->duals->link[i * F + j] = row_dual[off + i * F + j];
                }
            }
        }
        sol->duals->min_open_facilities =
            (m->use_min_open && m->min_open_row_index >= 0)
                ? row_dual[m->min_open_row_index] : 0.0;
        sol->has_duals = 1;
    }

    free(col_value); free(row_dual);
    return sol;
}

/* Solve as IP (binary on every column). No duals. */
lrsp_master_solution_t* lrsp_master_solve_ip(
    lrsp_master_t* m, lrsp_arena_t* result_arena
) {
    if (!m || !result_arena) return NULL;
    int F = m->num_facilities;
    int total_cols = F + m->columns_count;

    set_all_integrality(m->highs, total_cols, kHighsVarTypeInteger);

    HighsInt rc = Highs_run(m->highs);
    (void)rc;

    lrsp_master_solution_t* sol =
        alloc_solution(result_arena, F, m->columns_count);
    if (!sol) return NULL;
    sol->model_status = (int)Highs_getModelStatus(m->highs);
    sol->is_optimal   = (sol->model_status == kHighsModelStatusOptimal);

    if (!sol->is_optimal) {
        sol->objective = NAN;
        return sol;
    }
    sol->objective = Highs_getObjectiveValue(m->highs);

    double* col_value = (double*)malloc(sizeof(double) * (size_t)total_cols);
    if (!col_value) return sol;
    Highs_getSolution(m->highs, col_value, NULL, NULL, NULL);

    for (int j = 0; j < F; ++j) sol->facility_open_values[j] = col_value[j];
    for (int k = 0; k < m->columns_count; ++k) {
        sol->column_values[k] = col_value[F + k];
        if (col_value[F + k] > 0.5) {
            sol->selected_column_indices[sol->selected_column_count++] = k;
        }
    }
    sol->has_duals = 0;
    free(col_value);
    return sol;
}
