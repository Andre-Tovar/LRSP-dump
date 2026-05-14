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
 * A_BNDS.C
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
 * appl_bounds
 */
 
     /*
int id;               /* identification of active minto 
double zlp;           /* value of the LP solution 
double *xlp;          /* values of the variables 
double zprimal;       /* value of the primal solution 
double *xprimal;      /* values of the variables 
int *vcnt;            /* variable for number of variables 
int *vind;            /* array for indices of variables
char *vclass;         /* array for type of bounds 
double *vvalue;       /* array for value of bounds
int bdim;             /* size of arrays 
	     */

unsigned int
appl_bounds (int id,
	     double zlp,
	     double *xlp,
	     double zprimal,
	     double *xprimal,
	     int *vcnt,
	     int *vind,
	     char *vclass,
	     double *vvalue,
	     int bdim)
{
  int j,n1,n2,n1n2,d;
  //  int dc;
  //int path;
  //int changed;
  BranchDec *decision;
  //Pair_VAR *temp;
  ColumnInfo *column;
  

  if (id) {// if recursive version of minto, no bounds are changed
   
      return (FAILURE);
  }

  if (counter == 1 || newnode == FALSE){
 
      return (FAILURE);
  }

  /*
  if (newnode == TRUE)
    newnode = FALSE;
  */

  //  col_generation=1;

  *vcnt = 0;

  inq_form();

  for (j=0; j < NFac; j++) {
    if (xstat[j] == REQUIRED) {
      // set lower and upper bounds to 1
      vind[*vcnt] = j;
      vclass[*vcnt] = 'L';
      vvalue[(*vcnt)++] = (double) 1;
      
      vind[*vcnt] = j;
      vclass[*vcnt] = 'U';
      vvalue[(*vcnt)++] = (double) 1;
      printf("T[%d] fixed to %lf\n", j, 1.0);
    } // if xstat[j] == REQUIRED

    if (xstat[j] == FORBIDDEN) {
      // set lower and upper bounds to 0
      vind[*vcnt] = j; 
      vclass[*vcnt] = 'L';
      vvalue[(*vcnt)++] = (double) 0;
      
      vind[*vcnt] = j;
      vclass[*vcnt] = 'U';
      vvalue[(*vcnt)++] = (double) 0;
      printf("T[%d] fixed to %lf\n", j, 0.0);
      
      // set all Y_{jk} variables to 0 too
      /*
      for (k=2*NFac+NCust+1; k < info_form.form_vcnt; k++) {
        inq_var(k, FALSE);
        sscanf(info_var.var_name, "Y_%d_%d", &dc, &path);
        if (dc == j) {
          vind[*vcnt] = k;
          vclass[*vcnt] = 'L';
          vvalue[(*vcnt)++] = (double) 0;

          vind[*vcnt] = k;
          vclass[*vcnt] = 'U';
          vvalue[(*vcnt)++] = (double) 0;
          fprintf(stdout, "%s fixed to %lf\n", info_var.var_name, 0.0);
        } // if dc == j
      } // for k
      */

    } // if xstat[j] == FORBIDDEN
  } // for j
  /////////////////////////////////////////////////////////////////

  //bounds of Vj variables
 
  for (j=0; j < NFac; j++) {
    if (xstat_l[j] ==LESS) {
      // update upper bound on vj
      vind[*vcnt] = j+NFac;
      vclass[*vcnt] = 'U';
      vvalue[(*vcnt)++] = (double) x_rhs_l[j];
      
      printf("V[%d]<= %d \n", j, x_rhs_l[j]);
      
      if ((x_rhs_l[j]==0)&& (xstat[j]==FREE)){
	printf("Since V[%d]<= %d, set T[%d]=0 \n", j, x_rhs_l[j],j);
	//set Tj=0 if not, since vj<=0
	
	xstat[j]=FORBIDDEN;
	vind[*vcnt] = j; 
	vclass[*vcnt] = 'L';
	vvalue[(*vcnt)++] = (double) 0;
	
	vind[*vcnt] = j;
	vclass[*vcnt] = 'U';
	vvalue[(*vcnt)++] = (double) 0;
	printf("T[%d] is fixed to %lf\n", j, 0.0);
	/*
	  // set all Y_{jk} variables to 0 too
	  for (k=2*NFac; k < info_form.form_vcnt; k++) {
	  inq_var(k, FALSE);
	  sscanf(info_var.var_name, "Y_%d_%d", &dc, &path);
	  if (dc == j) {
	  vind[*vcnt] = k;
	  vclass[*vcnt] = 'L';
	  vvalue[(*vcnt)++] = (double) 0;
	  
	  vind[*vcnt] = k;
	  vclass[*vcnt] = 'U';
	  vvalue[(*vcnt)++] = (double) 0;
	  fprintf(stderr, "%s fixed to %lf\n", info_var.var_name, 0.0);
	  } // if dc == j
	  } // for k
	  */
	
      } // x_rhs_l[j]==0
      
    } // if xstat_l[j] == LESS
    

    if (xstat_g[j] == GREATER) {
      // set lower and upper bounds to 0
      vind[*vcnt] = j+NFac; 
      vclass[*vcnt] = 'L';
      vvalue[(*vcnt)++] = (double) x_rhs_g[j];
      
      printf("V[%d]>= %d is added \n", j, x_rhs_g[j]);
         
      if ((xstat[j]==FREE)&&(x_rhs_g[j]>0)){ 
	printf("Since V[%d]>= %d, set T[%d]=1 \n", j, x_rhs_g[j],j);
	// set lower and upper bounds to 1
	xstat[j]=REQUIRED;
	vind[*vcnt] = j;
	vclass[*vcnt] = 'L';
	vvalue[(*vcnt)++] = (double) 1;
      
	vind[*vcnt] = j;
	vclass[*vcnt] = 'U';
	vvalue[(*vcnt)++] = (double) 1;
	fprintf(stderr, "T[%d] fixed to %lf\n", j, 1.0);
      } // if xstat[j] == FREE
      
    } // if xstat[j] == GREATER
  } // for j

 
  //  fprintf(stdout,"Bounds on Y variable because of the Ryan and Foster branching \n");
  decision=Rules;
  while (decision) {
    n1=decision->N1;
    n2=decision->N2;
    if (decision->type==CONNECTED)
      n1n2=1;
    else 
      n1n2=0;
    
    for (j=0; j < NFac; j++) {
      column=Col_Array[j];
      while(column){
	d=get_variable_index (column, n1,n2,n1n2);
	if (d!=-1){
	  vind[*vcnt] = d; 
	  vclass[*vcnt] = 'L';
	  vvalue[(*vcnt)++] = (double) 0;
	  vind[*vcnt] = d;
	  vclass[*vcnt] = 'U';
	  vvalue[(*vcnt)++] = (double) 0;
	  // inq_var(d,NO);
	  //  fprintf(stdout, "%s fixed to %lf\n",  info_var.var_name, 0.0);
	  
	} //if d!=-1
	column=column->next;
      }//while column
    } //for j
    decision=decision->next;
  }//while decision
  
  return (SUCCESS);
}


