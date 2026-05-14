#include "internal.h"

#define MESPPRC_VERSION_STRING "0.1.0"

void mespprc_struct_sizes(mespprc_struct_sizes_t* out) {
    if (!out) return;
    out->struct_sizes = (uint64_t)sizeof(mespprc_struct_sizes_t);
    out->status_t     = (uint64_t)sizeof(mespprc_status_t);
    out->pointer      = (uint64_t)sizeof(void*);
    out->double_      = (uint64_t)sizeof(double);
    out->int_         = (uint64_t)sizeof(int);
}

const char* mespprc_version(void) {
    return MESPPRC_VERSION_STRING;
}
