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

extern "C"{
#include <stdio.h>
#include <stdlib.h>
//#include <math.h>
#include <string.h>
#include "header.h"
#include "minto.h"
}




/*
 * appl_delconstraints
 */
/*
int id;               /* identification of active minto 
int *ccnt;            /* variable for number of constraints to be deleted 
int *cind;            /* array for indices of constraints to be deleted 
*/

unsigned int
appl_delconstraints(int id,
		    int *ccnt,
		    int *cind)
{
  int numberofCONST, numofconDELETED,i;
  
  if (id) {// if recursive version of minto, no bounds are changed
    //printf("failure since id\n");
    return (FAILURE);
  }
  
  if (counter == 1 || newnode == FALSE){
    //printf("failure since counter and no newnode\n");
    return (FAILURE);
  }
  
  
  inq_form();
  numberofCONST=info_form.form_ccnt;
  numofconDELETED=0;
  for (i=numRows;i<numberofCONST;i++){
    inq_constr(i);
    if (info_constr.constr_status!=DELETED){
      numofconDELETED++;	
    }
  }//for i
    

  if (numofconDELETED==0){
    printf("failure since numofconDELETED==0\n");
    return (FAILURE);
  }
 
  else {
    //  nde=0;
    *ccnt=0;

    if (numRows + numofconDELETED != lp_ccnt()){
      printf("ERROR: Exit.numberofCONST != lp_ccnt()\n ");
      // exit(9);
    }

    for (i=numRows;i<numberofCONST;i++){
      inq_constr(i);
      // printf("info_constr.constr_status=%d\n",info_constr.constr_status);
      if ((info_constr.constr_status!=DELETED)||(info_constr.constr_status!=INACTIVE)){
	/*
	index=lp_cix(info_constr.constr_name);
	if (index<0) {
	  printf("Error in a_cons, the index of constraint is not true. \n");
	  exit(9);
	} 
	nde++;
	cind[(*ccnt)++]=index;
	*/
	cind[(*ccnt)++]=info_constr.constr_activeix;    
      }
    }//for i
    printf("number of constraints deleted, *ccnt=%d \n",*ccnt);
    return(*ccnt>0 ? SUCCESS : FAILURE);
  }

 
    
  //printf("numberof constraints in current model=%d, number of rows in original=%d, number that should be deleted=%d\n ",numberofCONST,numRows,nde);
    
 
      

   
  
}
