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
  GAP_CONSTRAINTS *Cut;
  int index;
  if (id) {// if recursive version of minto, no bounds are changed
    printf("failure since id\n");
    return (FAILURE);
  }
  
  /*
  if (counter == 1 || newnode == FALSE){
    printf("failure since counter and no newnode\n");
    return (FAILURE);
  }
  */
  if (newnode == FALSE){
    printf("failure since no newnode\n");
    return (FAILURE);
  }
  if (stepcount==77){
    sprintf(CurrMpsF,"%s77d.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }

  inq_form();
  numberofCONST=info_form.form_ccnt;
  numofconDELETED= numberofCONST-numRows-num_gap_cuts;
  
  printf("# of constraints in CURRENT FORM=%d, in ACTIVE FORM=%d \n",numberofCONST,lp_ccnt());
  if (numofconDELETED==0){
    printf("failure since numofconDELETED==0\n");
    return (FAILURE);
  }
 
  else {
    nde=0;
    *ccnt=0;
    
    for (i=numRows;i<numberofCONST;i++){
      printf("i=%d \n",i);
      inq_constr(i);
      if (info_constr.constr_status==ACTIVE)
	printf("Constraint status for %s is ACTIVE \n",info_constr.constr_name);
      else if (info_constr.constr_status==INACTIVE) 
	printf("Constraint status for %s is INACTIVE \n",info_constr.constr_name);
      else if (info_constr.constr_status==DELETED)
	printf("Constraint status for %s is DELETED \n",info_constr.constr_name);


      if (info_constr.constr_status==ACTIVE) {
	if ((info_constr.constr_type!=GLOBAL)){
	  //	if (info_constr.constr_name[0]!='G'){
	  nde++;
	  index=lp_cix(info_constr.constr_name);
	  printf("lp index of the constraint is %d (-1 if DEACTIVATED, -2 if ERROR,  1 if INACTIVE\n",index);
	  if (index==ERROR) {
	    printf("Error in reading the index. There is no name!\n");
	    exit(1);
	  }
	  else if ((index!=INACTIVE)&&(index!=DEACTIVATED)) {
	    cind[(*ccnt)++]=index;
	    //lp_cix((info_constr.constr_name));
	    printf("lp index of the constraint to be deleted =%d\n",lp_cix((info_constr.constr_name)));
	    printf("Constraint %s with current form index %d is deleted \n",info_constr.constr_name, i);
	  }
	  else 
	    printf("Constraint %s is INACTIVE or DELETED\n",info_constr.constr_name);
	  
	  //}
	}
	else 
	  printf("Constraint %s is global with current index %d \n",info_constr.constr_name, i);
      } // if active
      else 
	printf("Constraint %s 's status is deleted or inactive\n",info_constr.constr_name);
    }//for i
    printf("number of constraints deleted, *ccnt=%d \n",*ccnt);
    /*
    if (GapConList) {
      Cut=GapConList;
      while (Cut){
	if (Cut->id != Cut->order+numRows)
	  Cut->id = Cut->order+numRows;
	Cut=Cut->next;
      }
    } //if  (GapConList) 
    */
    if (*ccnt>0)
      return(SUCCESS);
    else 
      return(FAILURE); 

    //return(*ccnt>0 ? SUCCESS : FAILURE);
  }

 
    
  //printf("numberof constraints in current model=%d, number of rows in original=%d, number that should be deleted=%d\n ",numberofCONST,numRows,nde);
    
 
      

   
  
}
