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
 * A_FEAS.C
 */

#include <stdio.h>
#include "minto.h"

/*
 * appl_feas
 */
 
unsigned
appl_feasible (id, zlp, xlp)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double *xlp;          /* values of the variables */
{
    return (YES);
}
