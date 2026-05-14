/*
 * Top-level lrsp_solve entry. Mirrors lrsp_solver/solver.py.
 *
 * The actual CG loop lives in column_generation.c. This file is just the
 * dispatcher and surfaces the public ABI from include/lrsp.h.
 */

#include "internal.h"

extern lrsp_status_t lrsp_run_column_generation(
    const lrsp_instance_t* instance,
    const lrsp_solver_config_t* config,
    lrsp_result_t** out_result);

lrsp_status_t lrsp_solve(
    const lrsp_instance_t* instance,
    const lrsp_solver_config_t* config,
    lrsp_result_t** out_result
) {
    return lrsp_run_column_generation(instance, config, out_result);
}
