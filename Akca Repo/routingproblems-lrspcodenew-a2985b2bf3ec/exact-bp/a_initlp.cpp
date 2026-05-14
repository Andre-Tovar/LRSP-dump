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
 * A_INITLP.C
 */
extern "C"{
#include <stdio.h>
#include "minto.h"
}

/*
 * appl_initlp
 */
/*
int id;               /* identification of active minto 
int *colgen;          /* column generation indicator 
*/
unsigned int
appl_initlp (int id, int* colgen)
{

 fprintf(stderr,"entering a_initlp.c\n");

  if (id)   // if recursive version of MINTO, do not use column generation
     *colgen = FALSE;
  else
     *colgen = TRUE;
   
 return (CONTINUE);
}
