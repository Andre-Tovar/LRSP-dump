/*
 * LRSPInstance lifecycle and accessors. Mirrors lrsp_solver/instance.py.
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

static char* dup_cstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

lrsp_status_t lrsp_instance_create(
    int num_customers,
    int num_facilities,
    double vehicle_capacity,
    double vehicle_fixed_cost,
    double vehicle_operating_cost,
    double vehicle_time_limit,
    lrsp_instance_t** out_instance
) {
    if (!out_instance) return LRSP_ERR_INVALID_ARG;
    if (num_customers < 0 || num_facilities < 0) return LRSP_ERR_INVALID_ARG;
    *out_instance = NULL;

    lrsp_instance_t* inst = (lrsp_instance_t*)calloc(1, sizeof(*inst));
    if (!inst) return LRSP_ERR_NOMEM;

    inst->name = dup_cstr("lrsp_instance");
    if (!inst->name) {
        free(inst);
        return LRSP_ERR_NOMEM;
    }

    /* Hint capacities to avoid reallocation when the caller knows N up front. */
    int cap_c = num_customers > 0 ? num_customers : 4;
    int cap_f = num_facilities > 0 ? num_facilities : 2;
    inst->customers = (lrsp_customer_t*)calloc((size_t)cap_c, sizeof(lrsp_customer_t));
    inst->facilities = (lrsp_facility_t*)calloc((size_t)cap_f, sizeof(lrsp_facility_t));
    if (!inst->customers || !inst->facilities) {
        free(inst->customers);
        free(inst->facilities);
        free(inst->name);
        free(inst);
        return LRSP_ERR_NOMEM;
    }
    inst->num_customer_capacity = cap_c;
    inst->num_facility_capacity = cap_f;
    inst->num_customers = 0;
    inst->num_facilities = 0;

    inst->vehicle_capacity        = vehicle_capacity;
    inst->vehicle_fixed_cost      = vehicle_fixed_cost;
    inst->vehicle_operating_cost  = vehicle_operating_cost;
    inst->vehicle_time_limit      = vehicle_time_limit;
    inst->finalized               = 0;

    *out_instance = inst;
    return LRSP_OK;
}

void lrsp_instance_destroy(lrsp_instance_t* inst) {
    if (!inst) return;
    free(inst->customers);
    free(inst->facilities);
    free(inst->name);
    free(inst);
}

static int grow_customers(lrsp_instance_t* inst) {
    int new_cap = inst->num_customer_capacity * 2;
    if (new_cap < 4) new_cap = 4;
    lrsp_customer_t* p = (lrsp_customer_t*)realloc(
        inst->customers, (size_t)new_cap * sizeof(*p));
    if (!p) return -1;
    inst->customers = p;
    inst->num_customer_capacity = new_cap;
    return 0;
}

static int grow_facilities(lrsp_instance_t* inst) {
    int new_cap = inst->num_facility_capacity * 2;
    if (new_cap < 2) new_cap = 2;
    lrsp_facility_t* p = (lrsp_facility_t*)realloc(
        inst->facilities, (size_t)new_cap * sizeof(*p));
    if (!p) return -1;
    inst->facilities = p;
    inst->num_facility_capacity = new_cap;
    return 0;
}

lrsp_status_t lrsp_instance_add_customer(
    lrsp_instance_t* inst, int id, double x, double y, double demand
) {
    if (!inst) return LRSP_ERR_INVALID_ARG;
    if (inst->finalized) return LRSP_ERR_INVALID_ARG;
    if (inst->num_customers >= inst->num_customer_capacity) {
        if (grow_customers(inst) != 0) return LRSP_ERR_NOMEM;
    }
    lrsp_customer_t* c = &inst->customers[inst->num_customers++];
    c->id = id;
    c->x = x;
    c->y = y;
    c->demand = demand;
    return LRSP_OK;
}

lrsp_status_t lrsp_instance_add_facility(
    lrsp_instance_t* inst, int id, double x, double y,
    double opening_cost, double capacity
) {
    if (!inst) return LRSP_ERR_INVALID_ARG;
    if (inst->finalized) return LRSP_ERR_INVALID_ARG;
    if (inst->num_facilities >= inst->num_facility_capacity) {
        if (grow_facilities(inst) != 0) return LRSP_ERR_NOMEM;
    }
    lrsp_facility_t* f = &inst->facilities[inst->num_facilities++];
    f->id = id;
    f->x = x;
    f->y = y;
    f->opening_cost = opening_cost;
    f->capacity = capacity;
    return LRSP_OK;
}

lrsp_status_t lrsp_instance_set_name(lrsp_instance_t* inst, const char* name) {
    if (!inst) return LRSP_ERR_INVALID_ARG;
    char* copy = dup_cstr(name ? name : "");
    if (!copy) return LRSP_ERR_NOMEM;
    free(inst->name);
    inst->name = copy;
    return LRSP_OK;
}

lrsp_status_t lrsp_instance_finalize(lrsp_instance_t* inst) {
    if (!inst) return LRSP_ERR_INVALID_ARG;
    if (inst->num_customers <= 0 || inst->num_facilities <= 0) {
        return LRSP_ERR_INVALID_ARG;
    }
    inst->finalized = 1;
    return LRSP_OK;
}

int lrsp_instance_num_customers(const lrsp_instance_t* i) {
    return i ? i->num_customers : 0;
}
int lrsp_instance_num_facilities(const lrsp_instance_t* i) {
    return i ? i->num_facilities : 0;
}
double lrsp_instance_vehicle_capacity(const lrsp_instance_t* i) {
    return i ? i->vehicle_capacity : 0.0;
}
double lrsp_instance_vehicle_fixed_cost(const lrsp_instance_t* i) {
    return i ? i->vehicle_fixed_cost : 0.0;
}
double lrsp_instance_vehicle_operating_cost(const lrsp_instance_t* i) {
    return i ? i->vehicle_operating_cost : 1.0;
}
double lrsp_instance_vehicle_time_limit(const lrsp_instance_t* i) {
    return i ? i->vehicle_time_limit : -1.0;
}
int lrsp_instance_is_finalized(const lrsp_instance_t* i) {
    return i ? i->finalized : 0;
}
const char* lrsp_instance_name(const lrsp_instance_t* i) {
    return (i && i->name) ? i->name : "";
}

lrsp_status_t lrsp_instance_get_customer(
    const lrsp_instance_t* i, int idx,
    int* out_id, double* out_x, double* out_y, double* out_demand
) {
    if (!i) return LRSP_ERR_INVALID_ARG;
    if (idx < 0 || idx >= i->num_customers) return LRSP_ERR_INVALID_ARG;
    const lrsp_customer_t* c = &i->customers[idx];
    if (out_id)     *out_id     = c->id;
    if (out_x)      *out_x      = c->x;
    if (out_y)      *out_y      = c->y;
    if (out_demand) *out_demand = c->demand;
    return LRSP_OK;
}

lrsp_status_t lrsp_instance_get_facility(
    const lrsp_instance_t* i, int idx,
    int* out_id, double* out_x, double* out_y,
    double* out_opening_cost, double* out_capacity
) {
    if (!i) return LRSP_ERR_INVALID_ARG;
    if (idx < 0 || idx >= i->num_facilities) return LRSP_ERR_INVALID_ARG;
    const lrsp_facility_t* f = &i->facilities[idx];
    if (out_id)             *out_id             = f->id;
    if (out_x)              *out_x              = f->x;
    if (out_y)              *out_y              = f->y;
    if (out_opening_cost)   *out_opening_cost   = f->opening_cost;
    if (out_capacity)       *out_capacity       = f->capacity;
    return LRSP_OK;
}
