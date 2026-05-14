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
 * A_DELCONS.C
 */

#include <stdio.h>
#include "header.h"


/*
 * appl_delconstraints
 */

unsigned
appl_delconstraints (id, ccnt, cind)
int id;               /* identification of active minto */
int *ccnt;            /* variable for number of constraints to be deleted */
int *cind;            /* array for indices of constraints to be deleted */
{

  int numberofCONST, numofconDELETED,i,nde;
  
  if (id) {// if recursive version of minto, no bounds are changed
    printf("failure since id\n");
    return (FAILURE);
  }
  
  if (counter == 1 || newnode == FALSE){
    printf("failure since counter and no newnode\n");
    return (FAILURE);
  }
  
  
  inq_form();
  numberofCONST=info_form.form_ccnt;
  numofconDELETED= numberofCONST-numRows;
  
  if (numofconDELETED==0){
    printf("failure since numofconDELETED==0\n");
    return (FAILURE);
  }
 
  else {
    nde=0;
    *ccnt=0;
    
    for (i=numRows;i<numberofCONST;i++){
      inq_constr(i);
      if (info_constr.constr_status!=DELETED){
	nde++;
	cind[(*ccnt)++]=i;
      }
    }//for i
    printf("number of constraints deleted, *ccnt=%d \n",*ccnt);
    return(*ccnt>0 ? SUCCESS : FAILURE);
  }

 
    
  //printf("numberof constraints in current model=%d, number of rows in original=%d, number that should be deleted=%d\n ",numberofCONST,numRows,nde);
    
 
      

   
  
}
