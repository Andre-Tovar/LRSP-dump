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
 * A_CONS.C
 */

extern "C"{
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "header.h"
#include "minto.h"
}

/*
 * appl_constraints
 */
/*
int id;               /* identification of active minto 
double zlp;           /* value of the LP solution 
double *xlp;          /* values of the variables 
double zprimal;       /* value of the primal solution 
double *xprimal;      /* values of the variables
int *nzcnt;           /* variable for number of nonzero coefficients 
int *ccnt;            /* variable for number of constraints 
int *cfirst;          /* array for positions of first nonzero coefficients 
int *cind;            /* array for indices of nonzero coefficients 
double *ccoef;        /* array for values of nonzero coefficients 
char *csense;         /* array for senses 
double *crhs;         /* array for right hand sides 
int *ctype;           /* array for the constraint types: LOCAL or GLOBAL 
char **cname;         /* array for names 
int sdim;             /* length of small arrays 
int ldim;             /* length of large arrays 
*/

unsigned int
appl_constraints(int id,
		 double zlp,
		 double *xlp,
		 double zprimal,
		 double *xprimal,
		 int *nzcnt,
		 int *ccnt,
		 int *cfirst,
		 int *cind,
		 double *ccoef,
		 char *csense,
		 double *crhs,
		 int *ctype,
		 char **cname,
		 int sdim,
		 int ldim)
{


  CONSTRAINTS *addcon;
  // *addcon1;
  int i;
  //int j,d,fac,pairNO;
  //  char *s;
  // char ConName[BUFSIZE];
  int size;
  char *_cstore;
  int numberofCONST, numofNEWCON;
  ColumnInfo *column;
  int nx;
 
  
  if (id) {// if recursive version of minto, no constraints are added
    return (FAILURE);
  }

 if (counter == 1 || newnode == FALSE){
     return (FAILURE);
  }

  if (newnode == TRUE)
    newnode = FALSE;

  
  *ccnt=0;
  *nzcnt=0;

 
  if (!ConList)
    return(FAILURE);

  inq_form();
  
  numberofCONST=info_form.form_ccnt;
  numofNEWCON=0;
  for (i=numRows;i<numberofCONST;i++){
    inq_constr(i);
    if (info_constr.constr_status!=DELETED){
      numofNEWCON++;
      //printf("constraint with index %d is NOT DELETED \n",i);

    }
    //else
    //printf("constraint with index %d is DELETED \n",i);
  } //for i
 
  if (numofNEWCON!=0){

    printf("numberof constraints in current model=%d, number of rows in original=%d, number of non-deleted=%d \n ",numberofCONST,numRows,numofNEWCON);
    printf("ERROR, the previous constraints are not deleted \n");
    exit(1);
  }
    
  //add ALL constraints from ConList
  
  addcon=ConList;
  
  // size= NAME_SIZE*nnewcon;
  //printf("nnewcon=%d \n",nnewcon);

  size= NAME_SIZE;
  _cstore=0;
  //_cstore = (char *) calloc(size, sizeof(char));

  nx=0;

  while (addcon) {
    
    printf("CandFAC=%d, CandCUST=%d \n", addcon->facility,addcon->customer);
    
    cfirst[*ccnt]=*nzcnt;
    
    if (_cstore)
      _cstore=0;
    _cstore = (char *) calloc(size, sizeof(char));
    
    column=Col_Array[addcon->facility];
    while (column){
      if (column->sequence[addcon->customer]>0){
	cind[*nzcnt]=column->varindex;
	ccoef[(*nzcnt)++]=(double)1.0;
      } //if had a non zero coeff
      column=column->next;
    }
    if (addcon->type==LESSCON){
      crhs[*ccnt] = (double) 0;
      csense[*ccnt] = 'L';
      

      printf("A constraint is added: Customer %d CANNOT be assigned to facility %d \n", addcon->customer, addcon->facility);
      sprintf(&(_cstore[0]),"F%d_C%d_F%d",0,addcon->customer, addcon->facility);
      _cstore[size - 1] = '\0';
      cname[*ccnt] = _cstore;
      
    }
    
    else if (addcon->type==GREATERCON){
      crhs[*ccnt] = (double) 1;
      csense[*ccnt] = 'G';
    
      printf("A constraint is added: Customer %d MUST be assigned to facility %d \n", addcon->customer, addcon->facility);
      sprintf(&(_cstore[0]),"A%d_C%d_F%d",1,addcon->customer, addcon->facility);
      _cstore[size- 1] = '\0';
      cname[*ccnt] = _cstore;
      
    }
    
    
    ctype[(*ccnt)++]=LOCAL;
    addcon->index=nx+numRows;
    nx++;
    addcon=addcon->next;
  } //while addcon
  
  cfirst[*ccnt]=*nzcnt;
  
  //  if (_cstore) 
  //free(_cstore);
  //_cstore=0;

  return (SUCCESS);

  
  
}



