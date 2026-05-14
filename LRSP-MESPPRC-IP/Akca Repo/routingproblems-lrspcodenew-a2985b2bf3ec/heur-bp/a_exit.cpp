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
 * A_EXIT.C
 */
extern "C"{
#include <stdio.h>
#include "header.h"
#include <stdlib.h>
#include <string.h>
#include "minto.h"
}
/*
 * appl_exit
 */
/* 
int id;               /* identification of active minto 
double zopt;          /* value of the final solution 
double *xopt;         /* values of the variables 
*/
unsigned int
appl_exit(int id, double zopt, double *xopt)
{

  
  //  FILE *InfoCOL;
  //char Col_file[BUFSIZE];
  ColumnInfo *column;
  int i,j;
  double gap;
  //double upper_bound;

  FILE *InfoUpperBnd;
  char UpperBnd_file[BUFSIZE];
  FILE *InfoInitCols; 
  char InitCols_file[BUFSIZE];
  int tnumcols, tnumnz,tdemand;
  FILE *InfoNumCols; 
  char NumCols_file[BUFSIZE];
  double FacFixedCost, VehicleFixedCost, VariableCost;
  int tdem;

  if (id)
    return(CONTINUE);


  //WRITE all Columns to a file ...........

  sprintf(InitCols_file,"./newcolfiles/%s.init",inq_file());
  InfoInitCols=fopen(InitCols_file,"w");
  

  if((xopt)){
    FacFixedCost=0;
    VehicleFixedCost=0;
    VariableCost=0;
    for (i=0;i<NFac;i++){
      if (xopt[i]>0){
	printf("T[%d]=%2.1lf \n",i,xopt[i]);
	inq_var(i,NO);
	FacFixedCost=FacFixedCost+info_var.var_obj*xopt[i];
      }
    }
    for (i=NFac;i<2*NFac;i++){
      if (xopt[i]>0){
	printf("V[%d]=%2.1lf \n",i-NFac,xopt[i]);
	VehicleFixedCost=VehicleFixedCost+xopt[i]*VFcost;
      }
    }
    printf("FacFixedCost=%5.2lf \n", FacFixedCost);
    printf("VehicleFixedCost=%5.2lf \n",VehicleFixedCost);
    VariableCost=-zopt-FacFixedCost-VehicleFixedCost;
    printf("VariableCost=%5.2lf \n", VariableCost);
  }
  
  
  sprintf(UpperBnd_file,"./newcolfiles/%s.ub",inq_file());
  InfoUpperBnd=fopen(UpperBnd_file,"w");
  if (zopt>-heur_upper_bnd) {
    fprintf(InfoUpperBnd, "%lf \n",zopt );
    //if(MYUB!=0)
    // fprintf(InfoUpperBnd, "-1\n");  
  }
  else {
    fprintf(InfoUpperBnd, "%lf \n",-heur_upper_bnd);
    // fprintf(InfoUpperBnd, "-1\n");
  }
  fclose(InfoUpperBnd);




  tnumcols=0;
  tnumnz=0;
  for (i=0;i<NFac;i++){
    tnumcols=tnumcols+PairNum[i];
  }

  for (i=0;i<NFac;i++){
    column=Col_Array[i];
    while (column){
      tdemand=0;
      for (j=0;j<NCust;j++){
	if (column->sequence[j]){
	  tdemand=tdemand+Demand[j];
	  tnumnz++;
	}
      }
      fprintf(InfoInitCols, "%d  %5.1lf  %d ",i,column->Col_Time,tdemand);
      //  if((xopt)&&(MYUB==0)){
      if (xopt){
	if (xopt[column->varindex]>EPS) {
	  printf("Y_%d_%d : Time: %5.4lf, %d : ",i,column->Name,column->Col_Time,tdemand);
	
	}
      }

      for (j=0;j<NCust;j++){
	if (column->sequence[j]) {
	  fprintf(InfoInitCols, "%d %d ",j,column->sequence[j]);
	  // if((xopt)&&(MYUB==0)){
	  if (xopt){
	    if (xopt[column->varindex]>EPS)
	      printf("%d %d ",j,column->sequence[j]);
	  }
	}
      }//for j
      fprintf(InfoInitCols, " -1 \n");
      //      if ((xopt)&&(MYUB==0)){
      if (xopt){
	if (xopt[column->varindex]>EPS)
	  printf(" %d \n",tdemand);
      }

      column=column->next;
    }
  } //for i

  fclose(InfoInitCols);

  sprintf(NumCols_file,"./newcolfiles/%s.num",inq_file());
  InfoNumCols=fopen(NumCols_file,"w");
  
  fprintf(InfoNumCols, "%d  %d \n ",tnumcols,tnumnz);
  for (i=0;i<NFac;i++)
    fprintf(InfoNumCols, "%d \n",PairNum[i]); 
  fclose(InfoNumCols);


  /******************************Write columns to .col file ********************/
  /*
  sprintf(Col_file,"%s.col",inq_file());
  InfoCOL=fopen(Col_file,"w");
  fprintf(InfoCOL, "File name: %s\n",inq_file());
  fprintf(InfoCOL, "Column information:\n");

  for (i=0;i<NFac;i++){
    column=Col_Array[i];
    fprintf(InfoCOL, "\n **************FACILITY=%d *******************************************\n",i);
    while (column){
      fprintf(InfoCOL, "Y_%d_%d : ",i,column->Name);
      fprintf(InfoCOL, "%d: ", column->Col_Time);

      for (j=0;j<NCust;j++){
	if (column->sequence[j])
	  fprintf(InfoCOL, "%d:%d ,",j,column->sequence[j]);
      }//for j
      fprintf(InfoCOL, "\n");
      column=column->next;
    }
  } //for i

 fprintf(InfoCOL, "\n END *************************************\n");
  fclose(InfoCOL);
  */

  /********************************************************************/
  

 
  

  //  int j,i;
  //FILE *fp_log;
  //my_var *temp;
  /*
  fp_log=fopen("OPT_out","w");
  fprintf(fp_log, "File name: %s\n",inq_file());
  fprintf(fp_log, "OPTIMAL SOLUTION VALUE: %f\n",zopt);
  */
  /*  
if (xopt){
    // fprintf(fp_log,"OPTIMAL SOLUTION:\n");
    //fprintf(fp_log,"Number of Customers: %d, Number of Facilities: %d\n", NCust,NFac);
    //fprintf(fp_log,"Customers are numbered from (0..%d) and Facilities are (%d, ..%d)\n",NCust-1,NCust,NCust+NFac-1);
    for (inq_form(),j=0; j < info_form.form_vcnt; j++){
      if (xopt[j]!=0) {
	inq_var(j,NO);
	fprintf(fp_log," %s = %f\n",info_var.var_name,xopt[j]); 
	temp=VarArr;
	if (!VarArr)
	  fprintf(fp_log,"empty\n");
	while (temp) {
	  if (!(strcmp(temp->my_name,info_var.var_name))) {
	    fprintf(fp_log,"the path: [ ");
	    for(i=0;i<temp->size;i++){
	      fprintf( fp_log,"%d  ",temp->my_path[i]);
	    }
	    fprintf(fp_log,"]\n");
	    break;	
	  }
	  // else 
	    //printf("not same variable\n");

	  temp=temp->next;
	}
	fprintf(fp_log,"\n");
      }//if
    }
    }*/
  /*
  fclose(fp_log);
  if (last_soln) {
    temp=VarArr;
    while (temp){
      VarArr=temp->next;
      temp->next=0;
      free(temp->my_path);
      free(temp->my_name);
      free(temp);
      temp=VarArr;
    }
    }*/

 
  //  upper_bound=TrueUpperBound();
  //fprintf(stdout,"My True upper bound=%lf\n",upper_bound);
  
  // fprintf(stdout,"My True upper bound=%lf\n",-TrueUpperBound());
 
  gap=stat_gap();
  fprintf(stdout,"My Integrality gap=%lf \n",gap);
  
  fprintf(stdout,"My Best integer solution=%lf\n",zopt);
  //inq_form;
  //printf("Number of variables in the model is %d \n",info_form.form_vcnt);

  for (i=0;i<NFac;i++){
    column=Col_Array[i];
    while (column){
      Col_Array[i]=column->next;
      column->next=0;
      free(column->sequence);
      column->sequence=0;
      free(column);
      column= Col_Array[i];
    }
    Col_Array[i]=0;
  }
  free(Col_Array);
  Col_Array=0;

  //free(SetThreshold);
  free(max_num_labels); 
  free(DoCWHeurColumnGen);
  free(SKIPFAC);

  //SetThreshold=0;
  max_num_labels=0; 
  DoCWHeurColumnGen=0;
  SKIPFAC=0;

  //wrt_prob("test.mps");
return (CONTINUE);
}
