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
 * A_TERMND.C
 */

#include <stdio.h>
#include "minto.h"

/*
 * appl_terminatenode
 */
 
unsigned
appl_terminatenode (id, zlp, change, threshold)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double change;        /* change in last three iterations */
double *threshold;    /* double to hold the threshold used to detect tailing off */
{
    return (NO);
}
