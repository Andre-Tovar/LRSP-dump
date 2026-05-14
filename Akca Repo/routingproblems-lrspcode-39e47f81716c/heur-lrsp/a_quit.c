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
 * A_QUIT.C
 */

#include <stdio.h>
#include "minto.h"

/*
 * appl_quit
 */
 
unsigned
appl_quit (id, zprimal, xprimal)
int id;               /* identification of active minto */
double zprimal;       /* value of the final solution */
double *xprimal;      /* values of the variables */
{
    return (CONTINUE);
}
