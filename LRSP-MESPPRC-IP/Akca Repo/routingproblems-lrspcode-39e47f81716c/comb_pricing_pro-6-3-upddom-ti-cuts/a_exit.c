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

#include "header.h"

/*
 * appl_exit
 */
 
unsigned
appl_exit (id, zopt, xopt)
int id;               /* identification of active minto */
double zopt;          /* value of the final solution */
double *xopt;         /* values of the variables */
{

  FILE *InfoCOL;
  char Col_file[BUFSIZE];
  ColumnInfo *column;
  int i,j;
  double gap;
  double upper_bound,LB;
  GAP_CONSTRAINTS *Cut, *Cut1;
  

  if (id)
    return(CONTINUE);

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
      if (xopt[column->varindex]>0){
	printf("Y_%d_%d : ",i,column->Name);
	printf("%d: ", column->Col_Time);
      }
      
      for (j=0;j<NCust;j++){
	if (column->sequence[j]){
	  fprintf(InfoCOL, "%d:%d ,",j,column->sequence[j]);
	  if (xopt[column->varindex]>0){
	    printf("%d:%d ,",j,column->sequence[j]);
	  }
	}
      }//for j
      fprintf(InfoCOL, "\n");
      if (xopt[column->varindex]>0){
	printf("\n");
      }
      column=column->next;
    }
  } //for i


  

  fprintf(InfoCOL, "\n END *************************************\n");
  fclose(InfoCOL);
  
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

  gap=stat_gap();
  //upper_bound=TrueUpperBound();
  printf("My Integrality gap=%lf \n",gap);
  printf("My Best integer solution=%lf\n",zopt);
  LB=(1-gap*0.01)*zopt;
  printf("My Best LOWER Bnd=%lf\n",LB);

  //inq_form;
  //printf("Number of variables in the model is %d \n",info_form.form_vcnt);

  
  free(SetThreshold);
  free(max_num_labels); 
  free(DoCWHeurColumnGen);
  free(SKIPFAC);

  SetThreshold=0;
  max_num_labels=0; 
  DoCWHeurColumnGen=0;
  SKIPFAC=0;
  

  Cut=GapConList;
  while (Cut) {
    Cut1=Cut->next;
    free(Cut);
    Cut =Cut1;
  }
  GapConList=0;
 

return (CONTINUE);
}
