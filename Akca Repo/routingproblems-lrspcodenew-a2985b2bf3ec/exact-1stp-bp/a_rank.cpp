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
 * A_RANK.C
 */
extern "C"{
#include <stdio.h>
#include "minto.h"
}

/*
 * appl_rank
 */
/*
int id;               /* identification of active minto 
long depth;           /* identification: depth 
long creation;        /* identification: creation 
double zlp;           /* value of the LP solution 
double zprimal;       /* value of the primal solution 
double *rank;         /* variable for rank value 
*/

unsigned int
appl_rank(int id,
	  long depth,
	  long creation,
	  double zlp,
	  double zprimal,
	  double *rank)
{
 //if the rule is best LP then  
 return (FAILURE);
 /* 
  //if dept first:
  *rank=(double)creation;
  return(SUCCESS);
 */

 
}
