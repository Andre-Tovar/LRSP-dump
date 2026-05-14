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
 * A_FATHOM.C
 */
#include "header.h"

/*
 * appl_fathom  
 */
 
unsigned
appl_fathom (id, zlp, zprimal)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double zprimal;       /* value of the primal solution */
{

  /*  
  if (fathomnode) {
    fathomnode=0;
    fprintf(stderr,"fathomming since tere is artificial variables in the solution \n");
    return (SUCCESS);
  }
  */

    return (FAILURE);
}
