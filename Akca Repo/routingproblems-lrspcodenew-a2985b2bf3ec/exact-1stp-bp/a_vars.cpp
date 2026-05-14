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
 * A_VARS.C
 */
///////////////////////////////////////////////////
//changed to solve Elementary shortest path
// The ESPRC algorithm is rewritten  
//             July26
//////////////////////////////////////////////////
//add 1 column at a time

extern "C"{
#include "header.h"
#include "minto.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ilcplex/cplex.h> 
#include <time.h>
}

extern CPXENVptr env;
extern CPXLPptr lp;


typedef struct FALIST{
  int  custind;
  int conind;
  struct FALIST *next;
} FacAssgnList;

#define MY_EPS 0.000001

int *ChooseCustSet (int ncust, int fac, double **redCost, int size){

  int *custset;
  int *set;
  int source_x,index,i,j, index2;
  double min = MY_INF;
  double min2 = MY_INF;
  int dim, counter;
  double min_inside;


  if (size==0)
    return 0;

  custset = (int *)calloc(ncust,sizeof(int));
  
  if (size>ncust){
    size=ncust;
    for (i=0;i<ncust;i++)
      custset[i]=1;
    return(custset);
  }

  dim=size+1;
  set = (int *)calloc(dim,sizeof(int));
  source_x=ncust+1;
  index = -1;
  index2 = -1;

  //choose 2 first customers
  if (redCost[source_x][0] <= redCost[source_x][1] + MY_EPS){
    min = redCost[source_x][0] ;
    index = 0;
    min2 = redCost[source_x][1];
    index2 = 1;
  }
  else {
    min2 = redCost[source_x][0] ;
    index2 = 0;
    min = redCost[source_x][1];
    index = 1;
  }

  //choose first 2 customers
 
  for (i=2;i<ncust;i++){
    //printf("redCost[source][%d]=%lf \n",i,redCost[source_x][i]);
    if (min >= redCost[source_x][i] + MY_EPS) {
      min2 = min;
      index2 = index;
      min = redCost[source_x][i];
      index=i;
    }
    else if (min2 >= redCost[source_x][i] + MY_EPS) {
      min2 = redCost[source_x][i];
      index2 = i;
    }
  }

  if ((index==-1)||(index2 == -1)) {
    printf("ERROR in ChooseCustSet, first customer cannot be found \n");
    exit(9);
  }
  else {
    set[0]=index;
    set[1]=index2;
    set[2]=-1;
    custset[index]=1;
    custset[index2] = 1;
    counter=2;
    //printf("index=%d \n", index);
  }

  //choose other customers
  while (counter<size){
    // printf("counter=%d \n",counter);
    min=MY_INF;
    index=-1;
    for (i=0;i<ncust;i++){
      if (custset[i]==0){ //not in the list
	//printf("redCost[%d][%d]=%lf, redCost[%d][%d]=%lf, \n",set[0],i,redCost[set[0]][i],i,set[0],redCost[i][set[0]]);
	if (redCost[set[0]][i]<=redCost[i][set[0]] + MY_EPS)
	  min_inside=redCost[set[0]][i];
	else
	  min_inside=redCost[i][set[0]];

	for (j=1;j<dim;j++){
	  if (set[j]==-1)
	    break;
	  // printf("redCost[%d][%d]=%lf, redCost[%d][%d]=%lf, \n",set[j],i,redCost[set[j]][i],i,set[j],redCost[i][set[j]]);
	
	  if (min_inside>=redCost[set[j]][i]+MY_EPS)
	    min_inside=redCost[set[j]][i];
	  if (min_inside>=redCost[i][set[j]]+MY_EPS)
	    min_inside=redCost[i][set[j]];
	} //for j
	if (min >= min_inside+MY_EPS){
	  min=min_inside;
	  index=i;
	}
      }
    } //for i

    if (index!=-1){
      set[counter]=index;
      counter++;
      set[counter]=-1;
      custset[index]=1;
      //printf("index=%d \n", index);
    }
    else {
      printf("No more customers is found. Exit the function. Number of customers in the set is %d  \n",counter);
      break;
    }
  } //end of while
  
  free(set);
  set=0;
  /*
  printf("Customers in the set are: ");
  for (i=0;i<NCust;i++){
    if (custset[i]==1)
      printf("%d ", i);
  }		
  printf("\n");
  */
  return(custset);

} 


/*
 * appl_variables
 */
/*
int id;               /* identification of active minto 
double zlp;           /* value of the LP solution 
double *xlp;          /* values of the variables 
double zprimal;       /* value of the primal solution 
double *xprimal;      /* values of the variables 
int *nzcnt;           /* variable for number of nonzero coefficients 
int *vcnt;            /* variable for number of variables 
char *vclass;          /* array for classification of variables 
double *vobj;         /* array for objective coefficients of variables added 
double *varlb;        /* array for lower bounds of vars added 
double *varub;        /* array for upper bounds of vars added 
int *vfirst;          /* array for positions of first nonzero coefficients 
int *vind;            /* array for indices of nonzero coefficients 
double *vcoef;        /* array for values of nonzero coefficients 
char **vname;         /* array for names 
int sdim;             /* length of small arrays 
int ldim;             /* length of large arrays 
*/
unsigned int
 appl_variables(int id,
		double zlp,
		double *xlp,
		double zprimal,
		double *xprimal,
		int *nzcnt,
		int *vcnt,
		char *vclass,
		double *vobj,
		double *varlb,
		double *varub,
		int *vfirst,
		int *vind,
		double *vcoef,
		char **vname,
		int sdim,
		int ldim)
{
  
  
  // char filename[BUFSIZ];
  int i, j, k, m, t, size_arr;
  // int numRows = NFac + NCust+NCust*NFac;
  double TotalNeg;
  int FAC;
  int Soln;
  int minnum,nonegcol;
  //double TotalDemand, length;
  //double *TotalDemand, *length;
  int TotalDemand;
  // int length;
  //  int **pathArr;
  // int *pathArr;
  double *duals;
  double *pi;
  double *mu;
  double **sigma;
  double *v;
  double totfac;
  // int **d;
  double **d;
  double **rc;
  // double redcost;
  // int **Path;
  // int *Path;
  //  char OutFile[BUFSIZE];
  //double sum;  
  int numcol;
  ColumnInfo *column;
  int generate;
  BranchDec *decision;
  LABEL_NODE *SinkLabels,*temp1,*temp2;
  //double tcost;
  int numberofCONST, numofNEWCON;
  double *rule4duals;
  int nx;
  CONSTRAINTS *addcon;
  FacAssgnList **LISTofCUST, *coninfo;
  //double threshold;
  // int max_num_labels; 
  // int unreachlimit;
  //  int set_threshold;
  //int *SolveExactPricing;

  // double f;
  //LABEL_NODE *ColList;
  int doagain;
  //  LIST_VAR_INDEX *Datafield;
  //int index;
 int *solved;
 int BranchingRF;
 double total_time, red_cost;
 int *CustomerSet;
 int SizeOfCustSet,i1;
 int Choose_Cust_Set;
 // time_t start,end; //for seconds
 clock_t start,end,t1,t2; //for CPU sec
 double dif;
 int time_mes;

 solved=0;
 time_mes = 0;
  printf("stepcount = %d, numsubsetpri = %d\n",stepcount, numsubsetpri);

 
  if (id) { // if recursive version of MINTO, no columns should be generated
    //fprintf(stderr,"id olunca ciktiiii\n");
    return (FAILURE);
  }
  
  if (counter > 1 && newnode){
    // printf("in a_vars.c, counter>1 and newnode, so do not do column generation\n");
    return (FAILURE);
  }

  /*
  if (stepcount > NUMCOLGEN) {

    if (counter == 1) {
      sprintf(CurrMpsF,"%s.mps",inq_file());
      //strcpy(CurrMpsF, "test.mps");
      wrt_prob(CurrMpsF);
    } // if counter == 1
    
    return (FAILURE);

  }
  */

  stepcount++;
  
  if (time_mes)
    t1=clock();

  BranchingRF=0;
  printf("solutionstat=%d \n",CPXgetstat(env,lp));
  
  generate=1;  
  if (CPXgetstat(env,lp)==CPX_STAT_INFEASIBLE){
    *vcnt=0;
    *nzcnt = 0;
    vfirst[0] = 0;
    printf("solution status is not feasible, do not do column generation! \n");
    generate=0;
    //wrt_prob("curr.mps");
    // exit(9);
  }

  if (generate) {

    solved=0;
    inq_form();
    numberofCONST=info_form.form_ccnt;
    numofNEWCON=0;
    for (i=numRows;i<numberofCONST;i++){
      inq_constr(i);
      if (info_constr.constr_status!=DELETED) {
	numofNEWCON++;
	//	printf("Index for non-deleted constraint=%d and name=%s\n",i,info_constr.constr_name);
      }
      //else
      //printf("Index for deleted constraint=%d \n",i);
    } //for i
  
    if (numRows+numofNEWCON != lp_ccnt()) {
      printf("ERROR: Exit. Number of constraints in current formualtion is not equal to the number of constr. in active form. \n");
      // exit(9);
    }
    rule4duals =0;
    
    if (numofNEWCON!=0){
      printf("number of constraints=%d , number of initial rows=%d , number of new (non deleted)=%d\n", numberofCONST,numRows,numofNEWCON);
      rule4duals = (double *) calloc(numofNEWCON, sizeof(double));
      
      addcon=ConList;
      while (addcon){
	inq_constr(addcon->index);
	if (info_constr.constr_status==ACTIVE) {
	  /*
	  index=lp_cix(info_constr.constr_name);
	  if (index<0){
	    printf("Error in reading the index in a_vars.c.\n");
	    exit(9);
	  }
	  rule4duals[addcon->index-numRows]=-lp_pi(index); 
	  */
	  rule4duals[addcon->index-numRows]=-lp_pi(info_constr.constr_activeix);
	}
	else 
	  rule4duals[addcon->index-numRows]=0;
	addcon=addcon->next;
      }//while

      for (m=0; m < numofNEWCON ; m++){
	printf("rule4duals[%d]=%lf \n",m,rule4duals[m]);   
      }
      // exit(1);
    } //if (numofNEWCON!=0)
    
   
    //create arrays for dual variables, distance reduced cost  
    duals = (double *) calloc(numRows, sizeof(double));   
 
    d = (double **) calloc(NCust+2, sizeof(double *));
    rc = (double **) calloc(NCust+2, sizeof(double *));
     for (i = 0; i < NCust+2; i++) {
       d[i] = (double *) calloc(NCust+2, sizeof(double));
       rc[i] = (double *) calloc(NCust+2, sizeof(double));
     } // for i
 
     // fprintf(DUALS, "Stepcount=%d:\n",stepcount);
     inq_form();
     for (m=0;m<numRows;m++){
       inq_constr(m);
       if (info_constr.constr_status==ACTIVE) {
	 /*
	 index=lp_cix(info_constr.constr_name);
	 if (index<0){
	   printf("Error in reading the index in a_vars.c.\n");
	   exit(9);
	 }
	 duals[m] = -lp_pi(index); 
	 */
	 duals[m] = -lp_pi(info_constr.constr_activeix);
       }
       else
	 duals[m] =0;
     } //for i


     // stepcount=stepcount+1;
    //printf("stepcount=%d\n",stepcount);
    
    pi = (double *) calloc(NCust, sizeof(double));
    mu = (double *) calloc(NFac, sizeof(double));
    sigma = (double **) calloc(NFac, sizeof(double *));
    for (i=0; i < NFac; i++)
      sigma[i] = (double *) calloc(NCust, sizeof(double));
    v = (double *) calloc(NFac, sizeof(double));
    
   
    for (i=0; i < NCust; i++) {
      pi[i] = duals[i];
      //if ((stepcount==16) || (stepcount==17)||(stepcount==18)||(stepcount==19)||(stepcount==22)||(stepcount==23)||(stepcount==24))
      //printf("pi[%d]=%lf; \n", i,pi[i]);
    } // for i
    m = NCust;
    for (i=0; i < NFac; i++) {
      mu[i] = duals[m];
      //if ((stepcount==16) || (stepcount==17)||(stepcount==18)||(stepcount==19)||(stepcount==22)||(stepcount==23)||(stepcount==24))
      //printf("mu[%d]=%lf; \n", i,mu[i]);
      m=m+1;
    } // for i
    
    for (j=0; j < NFac; j++) {
      for (i=0; i < NCust; i++) {
	sigma[j][i] = duals[m];
	//if ((stepcount==16) || (stepcount==17)||(stepcount==18)||(stepcount==19)||(stepcount==22)||(stepcount==23)||(stepcount==24))
	//printf("sigma[%d][%d]=%lf; \n", j,i,sigma[j][i]);
	m=m+1;
      } // for i
    } // for j
    totfac=duals[m];
    m=m+1;
    for (i=0; i < NFac; i++) {
      v[i] = duals[m];
      
      printf("v[%d] = %lf;\n",i,v[i]);
      m=m+1;
    } // for i
    
    
    //NumColGen=NumColGen+1;
    *vcnt = 0;
    *nzcnt = 0;
    vfirst[0] = 0;
    
    //minnum=1;
    // minnum = 20;
    minnum=40; //number of columns to be found
    //minnum = 100;

    size_arr=minnum*NFac;
   
    SizeOfCustSet = CUSTSETSIZE1;
    /*
    if (numsubsetpri > NUMSUBSETPRIC1) {
      if (numsubsetpri < NUMSUBSETPRIC)
	SizeOfCustSet=CUSTSETSIZE2;
    
    
    }
    */

    /*
    if (numsubsetpri > NUMSUBSETPRIC1) {
      if (numsubsetpri > NUMSUBSETPRIC3)
	SizeOfCustSet=CUSTSETSIZE4;
      else if (numsubsetpri > NUMSUBSETPRIC2)
	SizeOfCustSet=CUSTSETSIZE3;
      else
	SizeOfCustSet=CUSTSETSIZE2;
    }
    */


    // SizeOfCustSet=CUSTSETSIZE1;
    CustomerSet=0;


    // vname=(char**)calloc (size_arr,sizeof(char*));
    for (j=0; j < size_arr; j++)
      vname[j] = (char *) calloc(NAME_SIZE, sizeof(char));
    
    
    //UPDATE THE RC BASED ON THE NEW CONSTRAINTS
    //TO DO THAT WE WILL ADJUST SIGMA[FAC][K] DEPENDING ON THE CONSTRAINT. 
    //IF WE FORBID THAT K IS ASSIGNED TO FAC, THEN SET SIGMA[FAC][K]=MY_INF
    //IF WE FORCE THAT K IS ASSIGNED TO FAC, THEN SET SIGMA[FAC][K]=SIGMA[FAC][K]-RULE4DUAL[..] 

   
    LISTofCUST=0;

    if(ConList){
      // fprintf(stdout,"Add constraints for branching rule 4 \n");
      addcon=ConList;
      LISTofCUST=(FacAssgnList **)calloc (NFac, sizeof(FacAssgnList*));
      nx=0;
      while (addcon){
	if (addcon->type==LESSCON){
	  sigma[addcon->facility][addcon->customer]=MY_INF;
	  
	} //if LESSCON
	
	if (addcon->type==GREATERCON){
	  sigma[addcon->facility][addcon->customer]=sigma[addcon->facility][addcon->customer]-rule4duals[addcon->index-numRows];
	  coninfo=(FacAssgnList *)calloc (1, sizeof(FacAssgnList)); 
	  coninfo->custind=addcon->customer;
	  coninfo->conind=addcon->index;
	  inq_constr(coninfo->conind);
	  if (info_constr.constr_status==DELETED) {
	    printf("ERROR: Wrong indice for a constraint, this constraint was deleted");
	    exit(1);
	  }
	  coninfo->next=LISTofCUST[addcon->facility];
	  LISTofCUST[addcon->facility]=coninfo;
	  coninfo=0;
	  //If there is a >= constraint for facility k, the customer cannot be assigned to other facilities != k
	  for (j=0;j<NFac;j++){
	    if (j!=addcon->facility)
	      sigma[j][addcon->customer]=MY_INF;
	  }//for j
	}
	
	addcon=addcon->next;
	nx++;
      }//while
      /*
      printf("total number of constraints in the list is nx=%d \n", nx);

      for (j=0;j<NFac;j++){
	if (LISTofCUST[j]) {
	  coninfo=LISTofCUST[j];
	  while (coninfo){
	    printf("GRATER THAN 1 constraints for facility=%d, customer=%d \n", j,coninfo->custind);
	    coninfo=coninfo->next;
	  }
	}
      }
      */

    } //if ConList

    // SolveExactPricing=(int *)calloc(NFac,sizeof(int));
    
    doagain=1;
    if (numsubsetpri > NUMSUBSETPRIC)
      Choose_Cust_Set=0;
    else{
      numsubsetpri++;
      Choose_Cust_Set=1;
    }

    if (ConList)
      Choose_Cust_Set=0;

    Choose_Cust_Set=0;

    if (!solved)
      solved=(int *)calloc (NFac,sizeof(int));
    for (j=0;j<NFac;j++)
      solved[j]=0;

    while (doagain){
      
      if (solve_pri_foronefac == 0){
	for (t=0; t < NFac; t++) {
	  
	  if (xstat[t] != FORBIDDEN) {
	    if ((SKIPFAC[t]!=1)&&(solved[t]==0)){
	      // create network for DC j
	      if (Choose_Cust_Set==0)
		solved[t]=1;
	      FAC = t;
	      source = NCust;
	      sink = NCust+1;
	      //fprintf(stdout, "Facility is %d \n", t);
	      
	      // fill the d (time required to travel links) and rc (cost) arrays: 
	      for (i=0; i < NCust; i++) {
		for (k=0; k < NCust; k++) {
		  if (i!=k){
		    // if (sigma[FAC][k]==MY_INF)
		    if ((sigma[FAC][k]<=MY_EPS+MY_INF)&&(sigma[FAC][k]>=-MY_EPS+MY_INF)) 
		      d[i][k]=MY_INF;
		    else
		      d[i][k] = dist[i][k]; 
		    rc[i][k] = VOcost/60*d[i][k] - pi[k] +mu[FAC]*Demand[k]+sigma[FAC][k];
		    
		  }
		  else {
		    d[i][k] = 0; 
		    rc[i][k] = 0;
		  }
		}
	      } 
	      
	      // assign costs to links between source and customers
	      for (i=0; i < NCust; i++) {
		//   if (sigma[FAC][i]==MY_INF)
		if ((sigma[FAC][i]<=MY_EPS+MY_INF)&&(sigma[FAC][i]>=-MY_EPS+MY_INF)) 
		  d[source][i]=MY_INF;
		else 
		  d[source][i] = dist[FAC+NCust][i];
		rc[source][i] = VOcost/60*d[source][i] - pi[i] +mu[FAC]*Demand[i]+sigma[FAC][i];
		d[i][source] = MY_INF;
		rc[i][source] = MY_INF;
	      } // for i
	      
	      // assign costs to links between Customers and sink
	      for (i=0; i < NCust; i++) {
		// if (sigma[FAC][i]==MY_INF)
		if ((sigma[FAC][i]<=MY_EPS+MY_INF)&&(sigma[FAC][i]>=-MY_EPS+MY_INF)) 
		  d[i][sink] = MY_INF;
		else 
		  d[i][sink] = dist[i][FAC+NCust];
		rc[i][sink] = VOcost/60*d[i][sink];
		// if (sigma[FAC][i]==MY_INF)
		if ((sigma[FAC][i]<=MY_EPS+MY_INF)&&(sigma[FAC][i]>=-MY_EPS+MY_INF)) 
		  d[sink][i] = MY_INF;
		else
		  d[sink][i] = dist[FAC+NCust][i];
		rc[sink][i] = VOcost/60*d[sink][i] - pi[i] +mu[FAC]*Demand[i]+sigma[FAC][i];
		
	      } // for i
	      
	      //different in previous, it was 0, so there is link between source and sink
	      d[source][sink] = MY_INF;
	      d[source][source]=0;
	      
	      rc[source][sink] = MY_INF;
	      rc[sink][sink]=0;
	      d[sink][source] = MY_INF;
	      d[sink][sink]=0;
	      rc[sink][source] = MY_INF;
	      
	      //Update the distance matrix if ryan and foster type branching	
	      if(Rules){
		//fprintf(stdout,"Ryan and Foster Branching:Update the d and rc matrix\n");
		BranchingRF=1;
		decision=Rules;
		while (decision){
		  if (decision->type==CONNECTED){
		    for (i=0;i<NCust+2;i++){
		      if ((i!=decision->N2)&&(i!=decision->N1)){
			d[decision->N1][i]=MY_INF;
			d[i][decision->N2]=MY_INF;
			rc[decision->N1][i]=MY_INF;
			rc[i][decision->N2]=MY_INF;
		      }
		    }
		  } //if connected
		  
		  if (decision->type==NOT_CONNECTED){
		    d[decision->N1][decision->N2]=MY_INF;
		    rc[decision->N1][decision->N2]=MY_INF;
		  }
		  
		  decision=decision->next;
		}//while
	      } //if rules
	      
	      nonegcol=0; //there may be columns with neg. red cost
	      TotalNeg=0;
	      for (i=0; i < NCust+2; i++) {
		for (k=0; k < NCust+2; k++) {
		  if (rc[i][k]<=-0.01){ 
		    TotalNeg=TotalNeg+rc[i][k];
		  }
		}
	      }
	      if (TotalNeg+VFcost-v[FAC]>-0.01){
		nonegcol=1; //no columns with neg. red. cost
	      }
	      
	      
	      
	      if (!nonegcol){
		
		SinkLabels=0;  
		printf("Choose_Cust_Set= %d \n",Choose_Cust_Set); 
		if (Choose_Cust_Set==1){
		  
		  //		  time (&start);
		  if (time_mes)
		    start=clock();
		  CustomerSet = ChooseCustSet (NCust, FAC, rc,SizeOfCustSet);
		  printf("Customers in the set are: ");
		  for (i1=0;i1<NCust;i1++){
		    if (CustomerSet[i1]==1)
		      printf("%d ", i1);
		  }		
		  printf("\n");
		  //time (&end);
		  if (time_mes){
		    end=clock();
		    //	  dif = difftime(end,start);
		    dif=(end-start)/(double)CLOCKS_PER_SEC;
		    printf("determine the customer set =  %lf CPU seconds\n",dif);
		  } 
		}
		//time (&start);
		if (time_mes)
		  start=clock();
		Soln= Generate_Columns(d,rc,Demand,source,sink, &SinkLabels, v[FAC], max_num_labels[FAC],FAC,BranchingRF,solve_pri_foronefac,Choose_Cust_Set,CustomerSet,SizeOfCustSet);		
		//time (&end);
		//dif = difftime(end,start);
		if (time_mes) {
		  end=clock();
		  dif=(end-start)/(double)CLOCKS_PER_SEC;
		  printf("Pricing problem = %lf CPU seconds\n",dif);
		}
		if (Soln) {  // if there is a solution, if not, soln=0
		  numcol=0;
		  temp1=SinkLabels;
		  while ((temp1)&&(numcol<minnum)){
		    if (temp1->cond=='C'){  
		      sprintf(vname[*vcnt], "Y_%d_%d", t, PairNum[FAC]);
		      
		      var_index++;

		      //*****************************************
		      column=(ColumnInfo *)calloc (1,sizeof(ColumnInfo));
		      column->sequence=(int *)calloc (NCust,sizeof(int));
		      column->Col_Time=temp1->labeldata.Time;
		      column->Name=PairNum[FAC];
		      column->varindex=var_index;
		      //******************************************
		      
		      TotalDemand=0;
		      printf("Y_%d_%d: [", t, PairNum[FAC]);
		      
		      for (i=0; i < NCust; i++) {
			if (temp1->labeldata.nodeseq[i]> 0) { 
			  
			  vind[*nzcnt] = i;
			  vcoef[(*nzcnt)++] = (double)1;
			  column->sequence[i]=temp1->labeldata.nodeseq[i];
			  
			  TotalDemand=TotalDemand+Demand[i]; 
			  printf("%d:%d, ",i,temp1->labeldata.nodeseq[i]);
			} // if 
		      } // for i
		      printf("], C=%lf, T=%5.1lf\n",temp1->labeldata.Cost+VFcost-v[FAC], temp1->labeldata.Time);
		      
		      
		      vind[*nzcnt] = NCust+FAC;
		      vcoef[(*nzcnt)++] = -(double)(TotalDemand);
		      
		      for (i=0;i<NCust;i++) {
			if (temp1->labeldata.nodeseq[i]>0) {
			  vind[*nzcnt] = NCust+NFac+FAC*NCust+i;
			  vcoef[(*nzcnt)++] = (double) -1;
			}
		      }
		      
		      vind[*nzcnt] = NCust+NFac+NCust*NFac+1+FAC;
		      vcoef[(*nzcnt)++] = 1;
		      
		      if(ConList){
			if (LISTofCUST[FAC]) {
			  coninfo=LISTofCUST[FAC];
			  while (coninfo){
			    if (temp1->labeldata.nodeseq[coninfo->custind]>0) {
			      //printf("The variable has non zero coeff in const %d \n", coninfo->conind);
			      //inq_constr(coninfo->conind);
			      /*
				index=lp_cix(info_constr.constr_name);
				if (index<0){
				printf("Error in reading the index in a_vars.c.\n");
				exit(9);
				}
				vind[*nzcnt] = index;
			      */
			      vind[*nzcnt] = coninfo->conind;
			      vcoef[(*nzcnt)++] =(double) 1;
			    } //if
			    coninfo=coninfo->next;
			  }//while
			}	//if LISTofCUST[FAC]
		      } //if ConList
		      
		      
		      vobj[*vcnt] = -(double) (VOcost/60*temp1->labeldata.Time+VFcost);
		      
		      varlb[*vcnt] =  0;
		      varub[*vcnt] =  1;
		      vclass[*vcnt] = 'B';
		      vfirst[++(*vcnt)] = *nzcnt;
		      PairNum[t]++;
		      numcol++;
		      
		      column->next=Col_Array[FAC];
		      Col_Array[FAC]=column;
		      column=0;
		      
		    } //if =C candidate
		    
		    temp2=temp1;
		    temp1=temp1->next;
		    if (temp1)
		      temp1->prev=0;
		    temp2->next=0;
		    free(temp2->labeldata.nodeseq);
		    free(temp2);
		    
		  }//while temp1 and numcol<minnum
		  while (temp1){
		    temp2=temp1;
		    temp1=temp1->next;
		    if (temp1)
		      temp1->prev=0;
		    temp2->next=0;
		    free(temp2->labeldata.nodeseq);
		    temp2->labeldata.nodeseq=0;
		    free(temp2);
		  } //while temp1
		  
		  printf("Number of columns added for facility %d is %d\n",t,numcol);
		  //	fprintf(stdout,"*vcnt: %d\n",*vcnt);
		  //fprintf(stderr,"*nzcnt: %d\n",*nzcnt);
		  
		} //if a soln exists   
		
		else {
		  //		fprintf(stdout, "Pricing problem results 0 columns for facility %d \n", t);
		  if (Choose_Cust_Set==0){
		    if (SKIPFAC[FAC]==0)
		      SKIPFAC[FAC]=1;
		  }
		}
		if (CustomerSet)
		  free(CustomerSet);
		CustomerSet=0;
	      } //if !nonegcol
	      else {
		//fprintf(stdout, "Skipping pricing problem for Faciality %d\n", t);
		if (SKIPFAC[FAC]==0)
		  SKIPFAC[FAC]=1;
	      }
	    }//if SKIPFAC[t]!=1
	    
	    //else 
	    // fprintf(stdout, "SKIPFAC=1, thus, skip pricing problem for Faciality %d\n", t);
	    
	  } //if xstat[t] != FORBIDDEN
	  //else //if t=forbidden
	  //fprintf(stdout, "Skipping pricing problem for Faciality %d, since it is FORBIDDEN\n", t);
	} // for t

	if (*vcnt==0) {
	  if (Choose_Cust_Set==1){
	    numsubsetpri = NUMSUBSETPRIC+1;
	    Choose_Cust_Set=0;
	    doagain=1;
	  }
	  else {
	    doagain=0;
	    for (t=0;t<NFac;t++){
	      if  (SKIPFAC[t]==1){
		SKIPFAC[t]=2;
		doagain=1;
	      }
	    }
	  }
	}
	else
	  doagain=0;
      }  //if (solve_pri_foronefac == 0){
      
      else {

	//chosenFAC_ind is the selected facility
	if ((chosenFAC_ind<0)||(chosenFAC_ind>=NFac))
	  chosenFAC_ind=0;

	//choose a facility 

	if (xstat[chosenFAC_ind] != FORBIDDEN) 
	  FAC=chosenFAC_ind;
	else {
	  FAC=-1;
	  for (t=chosenFAC_ind;t<NFac;t++){
	    if (xstat[t] != FORBIDDEN) {
	      FAC=t;
	      break;
	    }
	  }
	  if (FAC==-1){
	    for (t=0;t<chosenFAC_ind;t++){
	      if (xstat[t] != FORBIDDEN) {
		FAC=t;
		break;
	      }
	    } //for
	  }
	}
	printf("Choosen facility is %d \n", FAC);
	
	if (FAC==-1){
	  printf("Can not find any facility to do pricing. All facilities are FORBIDDEN? \n");
	  exit(9);
	}
	chosenFAC_ind=FAC;

	/****   CALL PRICING PROBLEM FOR FACILITY FAC       *****/
	source = NCust;
	sink = NCust+1;
	// fill the d (time required to travel links) and rc (cost) arrays: 
	for (i=0; i < NCust; i++) {
	  for (k=0; k < NCust; k++) {
	    if (i!=k){
	      // if (sigma[FAC][k]==MY_INF)
	      if ((sigma[FAC][k]<=MY_EPS+MY_INF)&&(sigma[FAC][k]>=-MY_EPS+MY_INF)) 
		d[i][k]=MY_INF;
	      else
		d[i][k] = dist[i][k]; 
	      rc[i][k] = VOcost/60*d[i][k] - pi[k] +mu[FAC]*Demand[k]+sigma[FAC][k];
	      
	    }
	    else {
	      d[i][k] = 0; 
	      rc[i][k] = 0;
	    }
	  }
	} 
	
	// assign costs to links between source and customers
	for (i=0; i < NCust; i++) {
	  //   if (sigma[FAC][i]==MY_INF)
	  if ((sigma[FAC][i]<=MY_EPS+MY_INF)&&(sigma[FAC][i]>=-MY_EPS+MY_INF)) 
	    d[source][i]=MY_INF;
	  else 
	    d[source][i] = dist[FAC+NCust][i];
	  rc[source][i] = VOcost/60*d[source][i] - pi[i] +mu[FAC]*Demand[i]+sigma[FAC][i];
	  d[i][source] = MY_INF;
	  rc[i][source] = MY_INF;
	} // for i
	
	// assign costs to links between Customers and sink
	for (i=0; i < NCust; i++) {
	  // if (sigma[FAC][i]==MY_INF)
	  if ((sigma[FAC][i]<=MY_EPS+MY_INF)&&(sigma[FAC][i]>=-MY_EPS+MY_INF)) 
	    d[i][sink] = MY_INF;
	  else 
	    d[i][sink] = dist[i][FAC+NCust];
	  rc[i][sink] = VOcost/60*d[i][sink];
	  // if (sigma[FAC][i]==MY_INF)
	  if ((sigma[FAC][i]<=MY_EPS+MY_INF)&&(sigma[FAC][i]>=-MY_EPS+MY_INF)) 
	    d[sink][i] = MY_INF;
	  else
	    d[sink][i] = dist[FAC+NCust][i];
	  rc[sink][i] = VOcost/60*d[sink][i] - pi[i] +mu[FAC]*Demand[i]+sigma[FAC][i];
	  
	} // for i
	
	//different in previous, it was 0, so there is link between source and sink
	d[source][sink] = MY_INF;
	d[source][source]=0;
	
	rc[source][sink] = MY_INF;
	rc[sink][sink]=0;
	d[sink][source] = MY_INF;
	d[sink][sink]=0;
	rc[sink][source] = MY_INF;
	
	//Update the distance matrix if ryan and foster type branching	
	if(Rules){
	  //fprintf(stdout,"Ryan and Foster Branching:Update the d and rc matrix\n");
	  BranchingRF=1;
	  decision=Rules;
	  while (decision){
	    if (decision->type==CONNECTED){
	      for (i=0;i<NCust+2;i++){
		if ((i!=decision->N2)&&(i!=decision->N1)){
		  d[decision->N1][i]=MY_INF;
		  d[i][decision->N2]=MY_INF;
		  rc[decision->N1][i]=MY_INF;
		  rc[i][decision->N2]=MY_INF;
		}
	      }
	    } //if connected
	    
	    if (decision->type==NOT_CONNECTED){
	      d[decision->N1][decision->N2]=MY_INF;
	      rc[decision->N1][decision->N2]=MY_INF;
	    }
	    
	    decision=decision->next;
	  }//while
	} //if rules
	
	nonegcol=0; //there may be columns with neg. red cost
	TotalNeg=0;
	for (i=0; i < NCust+2; i++) {
	  for (k=0; k < NCust+2; k++) {
	    if (rc[i][k]<=-0.01){ 
	      TotalNeg=TotalNeg+rc[i][k];
	    }
	  }
	}
	if (TotalNeg+VFcost-v[FAC]>-0.01){
	  nonegcol=1; //no columns with neg. red. cost
	}
	
	if (!nonegcol){
	  
	  SinkLabels=0;  
	  Choose_Cust_Set=0;
	  CustomerSet=0;
	  Soln= Generate_Columns(d,rc,Demand,source,sink, &SinkLabels, v[FAC], max_num_labels[FAC],FAC,BranchingRF,solve_pri_foronefac,Choose_Cust_Set,CustomerSet,SizeOfCustSet) ;
	  numcol=0;
	  if (Soln) {  // if there is a solution, if not, soln=0
	 
	    temp1=SinkLabels;
	    while ((temp1)&&(numcol<minnum)){
	      if (temp1->cond=='C'){  
		sprintf(vname[*vcnt], "Y_%d_%d", FAC, PairNum[FAC]);
		
		var_index++;
		
		//*****************************************
		column=(ColumnInfo *)calloc (1,sizeof(ColumnInfo));
		column->sequence=(int *)calloc (NCust,sizeof(int));
		column->Col_Time=temp1->labeldata.Time;
		column->Name=PairNum[FAC];
		column->varindex=var_index;
		//******************************************
		
		TotalDemand=0;
		printf("Y_%d_%d: [", FAC, PairNum[FAC]);
		
		for (i=0; i < NCust; i++) {
		  if (temp1->labeldata.nodeseq[i]> 0) { 
		    
		    vind[*nzcnt] = i;
		    vcoef[(*nzcnt)++] = (double)1;
		    column->sequence[i]=temp1->labeldata.nodeseq[i];
		    
		    TotalDemand=TotalDemand+Demand[i]; 
		    printf("%d:%d, ",i,temp1->labeldata.nodeseq[i]);
		  } // if 
		} // for i
		printf("], C=%lf, T=%5.1lf\n",temp1->labeldata.Cost+VFcost-v[FAC], temp1->labeldata.Time);
		
		      
		vind[*nzcnt] = NCust+FAC;
		vcoef[(*nzcnt)++] = -(double)(TotalDemand);
		
		for (i=0;i<NCust;i++) {
		  if (temp1->labeldata.nodeseq[i]>0) {
		    vind[*nzcnt] = NCust+NFac+FAC*NCust+i;
		    vcoef[(*nzcnt)++] = (double) -1;
		  }
		}
		
		vind[*nzcnt] = NCust+NFac+NCust*NFac+1+FAC;
		vcoef[(*nzcnt)++] = 1;
		
		if(ConList){
		  if (LISTofCUST[FAC]) {
		    coninfo=LISTofCUST[FAC];
		    while (coninfo){
		      if (temp1->labeldata.nodeseq[coninfo->custind]>0) {
			//printf("The variable has non zero coeff in const %d \n", coninfo->conind);
			//inq_constr(coninfo->conind);
			/*
			  index=lp_cix(info_constr.constr_name);
			  if (index<0){
			  printf("Error in reading the index in a_vars.c.\n");
			  exit(9);
			  }
			  vind[*nzcnt] = index;
			*/
			vind[*nzcnt] = coninfo->conind;
			vcoef[(*nzcnt)++] =(double) 1;
		      } //if
		      coninfo=coninfo->next;
		    }//while
		  }	//if LISTofCUST[FAC]
		} //if ConList
		
		
		vobj[*vcnt] = -(double) (VOcost/60*temp1->labeldata.Time+VFcost);
		
		varlb[*vcnt] =  0;
		varub[*vcnt] =  1;
		vclass[*vcnt] = 'B';
		vfirst[++(*vcnt)] = *nzcnt;
		PairNum[FAC]++;
		numcol++;
		
		column->next=Col_Array[FAC];
		Col_Array[FAC]=column;
		column=0;
		      
	      } //if =C candidate
	      temp1=temp1->next;
	    }//while temp1 and numcol<minnum
	  }//if a soln exists 
	  printf("Number of columns added for facility %d is %d\n",FAC,numcol);
	
	}//if (!nonegcol){
	
	if (SinkLabels){ //if the list is not empty
	  //Use these columns for other facilities
	  for (t=0;t<NFac;t++){
	    if ((xstat[t] != FORBIDDEN)&& (t!=chosenFAC_ind)){
	      numcol=0;
	      temp1=SinkLabels;
	      while (temp1){
		total_time= temp1->labeldata.Time - dist[chosenFAC_ind+NCust][temp1->labeldata.first_node]-dist[temp1->labeldata.last_node][chosenFAC_ind+NCust]+dist[t+NCust][temp1->labeldata.first_node]+dist[temp1->labeldata.last_node][t+NCust];
		
		red_cost= VOcost/60*total_time;
		for (i=0; i < NCust; i++) {
		if (temp1->labeldata.nodeseq[i]> 0)
		  red_cost=red_cost - pi[i] + mu[t]*Demand[i] + sigma[t][i];
		}
		temp1->labeldata.Cost= red_cost;
		if (red_cost+VFcost-v[t] < -MY_EPS) //neg red.cost column!!
		  temp1->cond='C';
		else
		  temp1->cond='P';
		
		temp1=temp1->next;
	      }
	      
	      //ReSort the list
	      temp1=Label_SORT(SinkLabels);
	      SinkLabels=temp1;
	      temp1=SinkLabels;
	      
	      while ((temp1)&&(numcol<minnum)){
		if (temp1->cond=='C'){  
		  sprintf(vname[*vcnt], "Y_%d_%d", t, PairNum[t]);
		  var_index++;
		  total_time= temp1->labeldata.Time - dist[chosenFAC_ind+NCust][temp1->labeldata.first_node]-dist[temp1->labeldata.last_node][chosenFAC_ind+NCust]+dist[t+NCust][temp1->labeldata.first_node]+dist[temp1->labeldata.last_node][t+NCust];
		  
		  //*****************************************
		  column=(ColumnInfo *)calloc (1,sizeof(ColumnInfo));
		  column->sequence=(int *)calloc (NCust,sizeof(int));
		  column->Col_Time=total_time;
		  column->Name=PairNum[t];
		  column->varindex=var_index;
		  //******************************************
		  
		  TotalDemand=0;
		  printf("Y_%d_%d: [", t, PairNum[t]);
		  
		  for (i=0; i < NCust; i++) {
		    if (temp1->labeldata.nodeseq[i]> 0) { 
		      vind[*nzcnt] = i;
		      vcoef[(*nzcnt)++] = (double)1;
		      column->sequence[i]=temp1->labeldata.nodeseq[i];
		      TotalDemand=TotalDemand+Demand[i]; 
		      printf("%d:%d, ",i,temp1->labeldata.nodeseq[i]);
		    } // if 
		  } // for i
		  printf("], C=%lf, T=%5.1lf\n",temp1->labeldata.Cost+VFcost-v[t], total_time);
		  
		  vind[*nzcnt] = NCust+t;
		  vcoef[(*nzcnt)++] = -(double)(TotalDemand);
		  
		  for (i=0;i<NCust;i++) {
		    if (temp1->labeldata.nodeseq[i]>0) {
		      vind[*nzcnt] = NCust+NFac+t*NCust+i;
		      vcoef[(*nzcnt)++] = (double) -1;
		    }
		  }
		  
		  vind[*nzcnt] = NCust+NFac+NCust*NFac+1+t;
		  vcoef[(*nzcnt)++] = 1;
		  
		  if(ConList){
		    if (LISTofCUST[t]) {
		      coninfo=LISTofCUST[t];
		      while (coninfo){
			if (temp1->labeldata.nodeseq[coninfo->custind]>0) {
			  //printf("The variable has non zero coeff in const %d \n", coninfo->conind);
			  //inq_constr(coninfo->conind);
			  /*
			    index=lp_cix(info_constr.constr_name);
			    if (index<0){
			    printf("Error in reading the index in a_vars.c.\n");
			    exit(9);
			    }
			    vind[*nzcnt] = index;
			  */
			  vind[*nzcnt] = coninfo->conind;
			  vcoef[(*nzcnt)++] =(double) 1;
			} //if
			coninfo=coninfo->next;
		      }//while
		    }	//if LISTofCUST[FAC]
		  } //if ConList
		
		  
		  vobj[*vcnt] = -(double) (VOcost/60*total_time+VFcost);
		  
		  varlb[*vcnt] =  0;
		  varub[*vcnt] =  1;
		  vclass[*vcnt] = 'B';
		  vfirst[++(*vcnt)] = *nzcnt;
		  PairNum[t]++;
		  numcol++;
		  
		  column->next=Col_Array[t];
		  Col_Array[t]=column;
		  column=0;
		} //if =C candidate
		temp1=temp1->next;
	      }//while temp1 and numcol<minnum
	      
	      printf("Number of columns added for facility %d is %d\n",t,numcol);
	      
	    } //	  if ((xstat[t] != FORBIDDEN)&& (t!=chosenFAC_ind)){
	  } //for (t=0;t<NFac;t++){
	  
	} //if SinkLabels

	//delete the list
	temp1=SinkLabels;
	while (temp1){
	  temp2=temp1;
	  temp1=temp1->next;
	  if (temp1)
	    temp1->prev=0;
	  temp2->next=0;
	  free(temp2->labeldata.nodeseq);
	  temp2->labeldata.nodeseq=0;
	  free(temp2);
	} //while temp1
	
	solved[chosenFAC_ind]=1;
	if (*vcnt==0) {
	  //solved[chosenFAC_ind]=1;
	  doagain=0;
	  for (t=0;t<NFac;t++){
	    if ((solved[t]==0)&&(xstat[t] != FORBIDDEN)){
	      doagain=1;
	      break;
	    }
	  }
	}
	else
	  doagain=0;

	//change the choosen facility
       	chosenFAC_ind++;
	if ((chosenFAC_ind<0)||(chosenFAC_ind>=NFac))
	  chosenFAC_ind=0;
	//choose a facility 

	if ((xstat[chosenFAC_ind] == FORBIDDEN)||(solved[chosenFAC_ind]==1)){ 
	  FAC=-1;
	  for (t=chosenFAC_ind+1;t<NFac;t++){
	    if ((xstat[t] != FORBIDDEN)&& (solved[chosenFAC_ind]==0)){
	      FAC=t;
	      break;
	    }
	  }
	  if (FAC==-1){
	    for (t=0;t<chosenFAC_ind;t++){
	      if ((xstat[t] != FORBIDDEN)&& (solved[chosenFAC_ind]==0)) {
		FAC=t;
		break;
	      }
	    } //for
	  }
	  chosenFAC_ind=FAC;
	}
	if (chosenFAC_ind==-1)
	  solve_pri_foronefac =0;
	  

      } //if (solve_pri_foronefac == 1){
     
    } //while doagain
    

   
    free(duals);
    for (i = 0; i < NCust+2; i++) {
      free(d[i]);
      free(rc[i]);
    } // for i
    free(d);
    free(rc);
    
    free( pi);
    free(mu);
    free(v);

    duals=0;
    d=0;
    rc=0;
    pi=0;
    mu=0;
    v=0;

    for (i=0; i < NFac; i++)
      free(sigma[i]);
    free(sigma);


    if(ConList){
      for (i=0; i < NFac; i++) {
	if (LISTofCUST[i]) {
	  coninfo=LISTofCUST[i];
	  while(coninfo){
	    LISTofCUST[i]=coninfo->next;
	    coninfo->next=0;
	    free(coninfo);
	    coninfo=LISTofCUST[i];
	  }
	}
	LISTofCUST[i]=0;
      }
      if (LISTofCUST){
	free(LISTofCUST);
      }
      LISTofCUST=0;
    }
    if (rule4duals)
      free(rule4duals);
    rule4duals=0;

     if (solved)
      free(solved);
    solved=0;
  } //IF GENERATE
  
  if (*vcnt!=0){
    printf("Number of columns found: %d\n", *vcnt);
    
  }
  
  else  {
    fprintf(stdout, "Final LP objective: %lf\n", zlp);
    fprintf(stdout,"No new column is generated\n");

    /*
    fathomnode=0;

    for (j=2*NFac;j<2*NFac+NCust+1;j++){ 
      if(xlp[j]>EPS) {
	fathomnode=1;
	break;
      }
    }
    */

    
    if (counter == 1) {
      sprintf(CurrMpsF,"%s.mps",inq_file());
      //strcpy(CurrMpsF, "test.mps");
      wrt_prob(CurrMpsF);
    } // if counter == 1
    
   

  } // if *vcnt == 0
  if (time_mes){
    t2=clock();
    dif=(t2-t1)/(double)CLOCKS_PER_SEC;
    printf("Total a_vars = %lf CPU seconds\n",dif);
  }
  //  fprintf(stdout,"exiting a_vars.c\n");
  
  return(*vcnt > 0 ? SUCCESS:FAILURE);
  
}
