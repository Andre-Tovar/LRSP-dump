/*
 *     MINTO - Mixed INTeger Optimizer
 *
 *     VERSION 3.0
 *
 *     Author:    M.W.P. Savelsbergh
 *                School of Industrial and Systems Engineering
 *                Georgia Institute of Technology
 *                Atlanta, GA 30332-0205
 *                U.S.A.
 *
 *     (C)opyright 1992-1994 - M.W.P. Savelsbergh
 */

/*
 * A_PREP.C
 */

#include "header.h"

/*
 * appl_preprocessing
 */

unsigned
appl_preprocessing (id)
int id;               /* identification of active minto */
{
  int j;

  /*
  inq_form();
  for (j=2*NFac; j < 2*NFac+NCust+1; j++) {
    inq_var(j, NO);
    //if (info_var.var_lb > info_var.var_ub-EPS) {
    if (info_var.var_ub < 1.0) {
      info_var.var_ub=(double) 1;
      set_var(j);
      fprintf(stderr, "Variable %d is changed to 1 in a_prep()!!!\n", j);
    } // if
  } // for j
  */

  return (CONTINUE);
}
