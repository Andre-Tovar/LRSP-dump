/*
 * Akca-style LRSP `.txt` parser. Mirrors lrsp_solver/instance.py:104-226.
 *
 * Format:
 *   line 1: <num_facilities> <num_customers>
 *   line 2: <opening_cost_1> ... <opening_cost_F>
 *   line 3: <vehicle_fixed_cost> <num_vehicles_per_facility>
 *   line 4: <vehicle_capacity> <facility_capacity> <vehicle_time_limit>
 *   next num_customers rows: <id> <x> <y> <service_time> <demand>
 *   next num_facilities rows: <id> <x> <y> 0 0
 *
 * Files lacking the third field on line 4 (vehicle_time_limit) belong to LRP
 * or VRP families and are intentionally rejected so the LRSP scheduling
 * component is always present.
 */

#include "internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read an entire file into a malloc'ed buffer. Caller frees. Returns NULL on
 * error. */
static char* slurp_file(const char* path, size_t* out_length) {
    if (!path || !out_length) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read] = '\0';
    *out_length = read;
    return buf;
}

/* Split a line into tokens by whitespace (spaces / tabs). Returns the number
 * of tokens. tokens[i] points into `line` (which is mutated to insert
 * NUL terminators). At most max_tokens are extracted. */
static int tokenize_inplace(char* line, char** tokens, int max_tokens) {
    int n = 0;
    char* p = line;
    while (*p && n < max_tokens) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        tokens[n++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
    return n;
}

/* Iterate non-empty (after trimming) lines from buf, calling `cb` for each.
 * The buffer is mutated in place to insert NUL terminators. */
static int next_nonblank_line(char* buf, size_t length, size_t* cursor, char** out_line) {
    while (*cursor < length) {
        char* start = buf + *cursor;
        /* find end of line */
        char* end = start;
        while (end < buf + length && *end != '\n' && *end != '\r' && *end != '\0') end++;
        size_t line_len = (size_t)(end - start);
        /* advance cursor past any line-terminator chars */
        size_t advance = line_len;
        while (end < buf + length && (*end == '\n' || *end == '\r')) {
            advance++;
            end++;
        }
        *cursor += advance;

        /* trim trailing whitespace */
        char* line_end = start + line_len;
        while (line_end > start && isspace((unsigned char)*(line_end - 1))) {
            line_end--;
        }
        *line_end = '\0';
        /* trim leading whitespace */
        char* line_start = start;
        while (*line_start && isspace((unsigned char)*line_start)) line_start++;
        if (*line_start == '\0') continue;  /* blank line, skip */
        *out_line = line_start;
        return 1;
    }
    return 0;
}

lrsp_status_t lrsp_instance_load_akca_txt(
    const char* path,
    double vehicle_operating_cost,
    lrsp_instance_t** out_instance
) {
    if (!path || !out_instance) return LRSP_ERR_INVALID_ARG;
    *out_instance = NULL;

    size_t length = 0;
    char* buf = slurp_file(path, &length);
    if (!buf) return LRSP_ERR_FILE_NOT_FOUND;

    /* Pre-collect all non-blank lines so we can index them. */
    /* Maximum we need is 4 (header) + customers + facilities. The file may
     * have plenty of slack; we cap at length/2 lines (each at least 1 char). */
    size_t max_lines = length / 2 + 16;
    char** lines = (char**)malloc(sizeof(char*) * max_lines);
    if (!lines) { free(buf); return LRSP_ERR_NOMEM; }

    size_t cursor = 0;
    size_t line_count = 0;
    char* line_ptr = NULL;
    while (next_nonblank_line(buf, length, &cursor, &line_ptr)) {
        if (line_count >= max_lines) break;
        lines[line_count++] = line_ptr;
    }

    if (line_count < 4) {
        free(lines); free(buf);
        return LRSP_ERR_PARSE;
    }

    /* line 1: num_facilities num_customers */
    char* tokens[64];
    int n = tokenize_inplace(lines[0], tokens, 64);
    if (n < 2) { free(lines); free(buf); return LRSP_ERR_PARSE; }
    int num_facilities = atoi(tokens[0]);
    int num_customers  = atoi(tokens[1]);
    if (num_facilities <= 0 || num_customers <= 0) {
        free(lines); free(buf);
        return LRSP_ERR_PARSE;
    }

    /* line 2: opening costs */
    n = tokenize_inplace(lines[1], tokens, 64);
    if (n < num_facilities) {
        free(lines); free(buf); return LRSP_ERR_PARSE;
    }
    double* opening_costs = (double*)malloc(sizeof(double) * (size_t)num_facilities);
    if (!opening_costs) { free(lines); free(buf); return LRSP_ERR_NOMEM; }
    for (int i = 0; i < num_facilities; ++i) {
        opening_costs[i] = atof(tokens[i]);
    }

    /* line 3: vehicle_fixed_cost num_vehicles_per_facility (we only consume the first). */
    n = tokenize_inplace(lines[2], tokens, 64);
    if (n < 1) { free(opening_costs); free(lines); free(buf); return LRSP_ERR_PARSE; }
    double vehicle_fixed_cost = atof(tokens[0]);

    /* line 4: vehicle_capacity facility_capacity vehicle_time_limit */
    n = tokenize_inplace(lines[3], tokens, 64);
    if (n < 3) {
        /* This is the LRP/VRP rejection path. Keep the error code distinct
         * so the CLI can print a friendly message. */
        free(opening_costs); free(lines); free(buf);
        return LRSP_ERR_PARSE;
    }
    double vehicle_capacity   = atof(tokens[0]);
    double facility_capacity  = atof(tokens[1]);
    double vehicle_time_limit = atof(tokens[2]);

    /* body: num_customers customer rows then num_facilities facility rows. */
    size_t body_start = 4;
    size_t body_end   = body_start + (size_t)num_customers + (size_t)num_facilities;
    if (line_count < body_end) {
        free(opening_costs); free(lines); free(buf);
        return LRSP_ERR_PARSE;
    }

    lrsp_instance_t* inst = NULL;
    lrsp_status_t st = lrsp_instance_create(
        num_customers, num_facilities,
        vehicle_capacity, vehicle_fixed_cost, vehicle_operating_cost,
        vehicle_time_limit, &inst);
    if (st != LRSP_OK) {
        free(opening_costs); free(lines); free(buf);
        return st;
    }

    /* Use the file path stem as the instance name. */
    {
        const char* slash = strrchr(path, '/');
        const char* bslash = strrchr(path, '\\');
        const char* base = slash;
        if (bslash && (!slash || bslash > slash)) base = bslash;
        if (base) base++;
        else      base = path;
        const char* dot = strrchr(base, '.');
        if (dot && dot > base) {
            size_t name_len = (size_t)(dot - base);
            char* name = (char*)malloc(name_len + 1);
            if (name) {
                memcpy(name, base, name_len);
                name[name_len] = '\0';
                lrsp_instance_set_name(inst, name);
                free(name);
            }
        } else {
            lrsp_instance_set_name(inst, base);
        }
    }

    for (int i = 0; i < num_customers; ++i) {
        n = tokenize_inplace(lines[body_start + (size_t)i], tokens, 64);
        if (n < 5) {
            lrsp_instance_destroy(inst);
            free(opening_costs); free(lines); free(buf);
            return LRSP_ERR_PARSE;
        }
        int    cid    = atoi(tokens[0]);
        double cx     = atof(tokens[1]);
        double cy     = atof(tokens[2]);
        /* tokens[3] is service_time, currently ignored on Python side. */
        double demand = atof(tokens[4]);
        st = lrsp_instance_add_customer(inst, cid, cx, cy, demand);
        if (st != LRSP_OK) {
            lrsp_instance_destroy(inst);
            free(opening_costs); free(lines); free(buf);
            return st;
        }
    }

    for (int j = 0; j < num_facilities; ++j) {
        n = tokenize_inplace(lines[body_start + (size_t)num_customers + (size_t)j],
                             tokens, 64);
        if (n < 3) {
            lrsp_instance_destroy(inst);
            free(opening_costs); free(lines); free(buf);
            return LRSP_ERR_PARSE;
        }
        int    fid = atoi(tokens[0]);
        double fx  = atof(tokens[1]);
        double fy  = atof(tokens[2]);
        st = lrsp_instance_add_facility(
            inst, fid, fx, fy,
            opening_costs[j], facility_capacity);
        if (st != LRSP_OK) {
            lrsp_instance_destroy(inst);
            free(opening_costs); free(lines); free(buf);
            return st;
        }
    }

    st = lrsp_instance_finalize(inst);
    if (st != LRSP_OK) {
        lrsp_instance_destroy(inst);
        free(opening_costs); free(lines); free(buf);
        return st;
    }

    free(opening_costs);
    free(lines);
    free(buf);

    *out_instance = inst;
    return LRSP_OK;
}
