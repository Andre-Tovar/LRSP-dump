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
extern "C"{
#include <stdio.h>
#include "minto.h"
}
unsigned
appl_quit (int id, double zprimal, double *xprimal)
{
    return (CONTINUE);
}
