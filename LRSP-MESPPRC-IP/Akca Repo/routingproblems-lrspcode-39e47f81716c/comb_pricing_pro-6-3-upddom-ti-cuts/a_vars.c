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

#include "header.h"

typedef struct FALIST{
  int  custind;
  int conind;
  struct FALIST *next;
} FacAssgnList;


/*
 * appl_variables
 */
 
unsigned
appl_variables (id, zlp, xlp, zprimal, xprimal,
    nzcnt, vcnt, vclass, vobj, varlb, varub, vfirst, vind, vcoef, vname,
    sdim, ldim)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double *xlp;          /* values of the variables */
double zprimal;       /* value of the primal solution */
double *xprimal;      /* values of the variables */
int *nzcnt;           /* variable for number of nonzero coefficients */
int *vcnt;            /* variable for number of variables */
char *vclass;          /* array for classification of variables */
double *vobj;         /* array for objective coefficients of variables added */
double *varlb;        /* array for lower bounds of vars added */
double *varub;        /* array for upper bounds of vars added */
int *vfirst;          /* array for positions of first nonzero coefficients */
int *vind;            /* array for indices of nonzero coefficients */
double *vcoef;        /* array for values of nonzero coefficients */
char **vname;         /* array for names */
int sdim;             /* length of small arrays */
int ldim;             /* length of large arrays */
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
  int **d;
  double **rc;
  // double redcost;
  // int **Path;
  // int *Path;
  //  char OutFile[BUFSIZE];
  //double sum;  
  int numopen,numcol;
  ColumnInfo *column;
  int generate;
  BranchDec *decision;
  LABEL_NODE *SinkLabels,*temp1,*temp2;
  double tcost;
  int numberofCONST, numofNEWCON;
  double *rule4duals;
  int nx;
  CONSTRAINTS *addcon;
  FacAssgnList **LISTofCUST, *coninfo;
  //double threshold;
  // int max_num_labels; 
  int unreachlimit;
  //  int set_threshold;
  //int *SolveExactPricing;

  double f;
  LABEL_NODE *ColList;
  int doagain;
  //  LIST_VAR_INDEX *Datafield;
  double *CutDuals;
  int Rule4Const;
  GAP_CONSTRAINTS *Cut;
  int p,p2;
  double *GAPCutDuals;
  char *s;


  stepcount++;
  printf("stepcount=%d,\n",stepcount);
  //  if (stepcount==2)
  // return(FAILURE);
 
  if (id) { // if recursive version of MINTO, no columns should be generated
    fprintf(stderr,"id olunca ciktiiii\n");
    return (FAILURE);
  }
  
  if (counter > 1 && newnode){
    printf("in a_vars.c, counter>1 and newnode, so do not do column generation\n");
    return (FAILURE);
  }
  

  
 
  fprintf(stdout,"solutionstat=%d \n",CPXgetstat(mintoenv,mintolp));

  generate=1;  
  if (CPXgetstat(mintoenv,mintolp)!=1){
    *vcnt=0;
    *nzcnt = 0;
    vfirst[0] = 0;
    fprintf(stdout,"solution status is not feasible, do not do column generation! \n");
    generate=0;
    //wrt_prob("curr.mps");
    // exit(9);
  }

  if (generate) {
    
    inq_form();
    numberofCONST=info_form.form_ccnt;
    numofNEWCON=0;
    for (i=numRows;i<numberofCONST;i++){
      inq_constr(i);
      if (info_constr.constr_status!=DELETED) {
	numofNEWCON++;
	printf("Index for non-deleted constraint=%d and name=%s, type =%s\n",i,info_constr.constr_name,info_constr.constr_type==GLOBAL ? "GLOBAL" : "LOCAL"  );
	printf("debug: constr index:: %d\n", lp_cix(info_constr.constr_name));

      }
      else
	printf("Index for deleted constraint=%d,  name=%s\n",i,info_constr.constr_name);
    } //for i
    
    /*
    if (numofNEWCON!=0){
      printf("number of constraints=%d , number of initial rows=%d , number of new (non deleted)=%d\n", numberofCONST,numRows,numofNEWCON);
      if (numofNEWCON< num_gap_cuts) {
	printf("Error! Number of new constraints<number of GAP cuts \n");
	exit(9);
      }
      
      addcon=ConList;
      while (addcon) {
	printf("index of cons=%d, fac=%d, cust=%d \n",addcon->index, addcon->facility, addcon->customer);
	addcon=addcon->next;
      }
    */

      if (num_gap_cuts>0){
	GAPCutDuals = (double *) calloc(num_gap_cuts, sizeof(double)); 
	
	Cut=GapConList;
	while (Cut) {
	  inq_constr(Cut->id);
	  if (info_constr.constr_status==ACTIVE) {
	    GAPCutDuals[Cut->order]=-lp_pi(lp_cix(info_constr.constr_name)); 
	  }
	  else 
	    GAPCutDuals[Cut->order]=0;
	  
	  Cut=Cut->next;
	}
      }

      inq_form();
      numberofCONST=info_form.form_ccnt;
      numofNEWCON=0;
      for (i=numRows;i<numberofCONST;i++){
	inq_constr(i);
	if ((info_constr.constr_type==LOCAL)&&(info_constr.constr_status!=DELETED)) {
	  numofNEWCON++;
	}
      } //for i
      
          // CutDuals = (double *) calloc(numofNEWCON, sizeof(double));
               
      Rule4Const=numofNEWCON;
      if (Rule4Const>0) {
	rule4duals = (double *) calloc(Rule4Const, sizeof(double));
     
	addcon=ConList;
	while (addcon){
	  inq_constr(addcon->index);
	  if (info_constr.constr_status==ACTIVE) {
	    rule4duals[addcon->index-numRows-num_gap_cuts]=-lp_pi(lp_cix(info_constr.constr_name)); 
	  }
	  else 
	    rule4duals[addcon->index-numRows-num_gap_cuts]=0;
	  addcon=addcon->next;
	}//while

      }//if (Rule4Const>0)
    
    
   
    //create arrays for dual variables, distance reduced cost  
    duals = (double *) calloc(numRows, sizeof(double));   
 
    d = (int **) calloc(NCust+2, sizeof(int *));
    rc = (double **) calloc(NCust+2, sizeof(double *));
     for (i = 0; i < NCust+2; i++) {
       d[i] = (int *) calloc(NCust+2, sizeof(int));
       rc[i] = (double *) calloc(NCust+2, sizeof(double));
     } // for i
    
 
     // fprintf(DUALS, "Stepcount=%d:\n",stepcount);
     inq_form();
     for (m=0;m<numRows;m++){
       inq_constr(m);
       if (info_constr.constr_status==ACTIVE) {
	 duals[m] = -lp_pi(lp_cix(info_constr.constr_name)); 
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
      // printf("pi[%d]=%lf \n", i,pi[i]);
    } // for i
    m = NCust;
    for (i=0; i < NFac; i++) {
      mu[i] = duals[m];
      //printf("mu[%d]=%lf \n", i,mu[i]);
      m=m+1;
    } // for i
    
    for (j=0; j < NFac; j++) {
      for (i=0; i < NCust; i++) {
	sigma[j][i] = duals[m];
	//printf("sigma[%d][%d]=%lf \n", j,i,sigma[j][i]);
	m=m+1;
      } // for i
    } // for j
    totfac=duals[m];
    m=m+1;
    for (i=0; i < NFac; i++) {
      v[i] = duals[m];
      
      //printf( "v[%d] = %lf\n",i,v[i]);
    m=m+1;
    } // for i
    
    /*
    inq_var(100,YES);
    printf("Name=%s \n",info_var.var_name);
    printf("Obj=%lf \n",info_var.var_obj);

    for (j=0; j < numRows; j++) 
      printf("Coef[%d]=%lf \n",j,info_var.var_coef[j]);

    printf("lp_rc[100]=%lf \n",lp_rc(100));
    exit(9);
    */

    NumColGen=NumColGen+1;
    *vcnt = 0;
    *nzcnt = 0;
    vfirst[0] = 0;
    
    minnum=40; //number of columns to be found
    size_arr=minnum*NFac;
    


    // vname=(char**)calloc (size_arr,sizeof(char*));
    for (j=0; j < size_arr; j++)
      vname[j] = (char *) calloc(NAME_SIZE, sizeof(char));
    
    
    //UPDATE THE RC BASED ON THE NEW CONSTRAINTS
    //TO DO THAT WE WILL ADJUST SIGMA[FAC][K] DEPENDING ON THE CONSTRAINT. 
    //IF WE FORBID THAT K IS ASSIGNED TO FAC, THEN SET SIGMA[FAC][K]=MY_INF
    //IF WE FORCE THAT K IS ASSIGNED TO FAC, THEN SET SIGMA[FAC][K]=SIGMA[FAC][K]-RULE4DUAL[..] 

   
    LISTofCUST=0;

    if(ConList){
      fprintf(stdout,"Add constraints for branching rule 4 \n");
      addcon=ConList;
      LISTofCUST=(FacAssgnList **)calloc (NFac, sizeof(FacAssgnList*));

      nx=0;
      while (addcon){
	if (addcon->type==LESSCON){
	  sigma[addcon->facility][addcon->customer]=MY_INF;
	  
	} //if LESSCON
	
	if (addcon->type==GREATERCON){
	  sigma[addcon->facility][addcon->customer]=sigma[addcon->facility][addcon->customer]-rule4duals[addcon->index-numRows-num_gap_cuts];
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
	} //if GREATERCON
	
	addcon=addcon->next;
	nx++;
      }//while

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
     

    } //if ConList





    // SolveExactPricing=(int *)calloc(NFac,sizeof(int));
    
    doagain=1;

    while (doagain){
      
      for (t=0; t < NFac; t++) {
	
	if (xstat[t] != FORBIDDEN) {
	  if (SKIPFAC[t]!=1){
	    // create network for DC j
	  
	    FAC = t;
	    source = NCust;
	    sink = NCust+1;
	    //fprintf(stdout, "current Facility is %d, source is %d, sink is %d\n", t, source,sink);
	    
	    // fill the d (time required to travel links) and rc (cost) arrays: 
	    for (i=0; i < NCust; i++) {
	      for (k=0; k < NCust; k++) {
		if (i!=k){
		  if (sigma[FAC][k]==MY_INF)
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
	      if (sigma[FAC][i]==MY_INF)
		d[source][i]=MY_INF;
	      else 
		d[source][i] = dist[FAC+NCust][i];
	      rc[source][i] = VOcost/60*d[source][i] - pi[i] +mu[FAC]*Demand[i]+sigma[FAC][i];
	      d[i][source] = MY_INF;
	      rc[i][source] = MY_INF;
	    } // for i
	    
	    // assign costs to links between Customers and sink
	    for (i=0; i < NCust; i++) {
	      if (sigma[FAC][i]==MY_INF)
		d[i][sink] = MY_INF;
	      else 
		d[i][sink] = dist[i][FAC+NCust];
	      rc[i][sink] = VOcost/60*d[i][sink];
	      if (sigma[FAC][i]==MY_INF)
		d[sink][i] = MY_INF;
	      else
		d[sink][i] = dist[FAC+NCust][i];
	      rc[sink][i] = VOcost/60*d[sink][i] - pi[i] +mu[FAC]*Demand[i]+sigma[FAC][i];
	      
	    } // for i
	    

	    //UPDATE THE REDUCED COST CONSIDERING DUALS FROM GAP-CUTS
	    if (GapConList) {
	      Cut=GapConList;
	      while (Cut){
		for (i=0; i < NCust; i++) {
		  if (Cut->CustSet[i]>0){
		    for (k=0; k < NCust+2; k++) {
		      if ((k!=i)&&(d[k][i]<MY_INF)) {
			if (Cut->fac==FAC)
			  rc[k][i]=rc[k][i]+Demand[i]*GAPCutDuals[Cut->order];
			else {
			  p=FCap-Cut->tdem+Demand[i];
			  if (p>0)
			    rc[k][i]=rc[k][i]+p*GAPCutDuals[Cut->order];
			} //else
		      } //if k!=i
		    } //for k
		  }//if (Cut->CustSet[i]>0){
		}//for i
		Cut=Cut->next;
	      }//while cut
	    }//if (GapConList) {



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
	      fprintf(stdout,"Ryan and Foster Branching:Update the d and rc matrix\n");
	      
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
	      Soln= Generate_Columns(d,rc,Demand,source,sink, &SinkLabels, v[FAC], max_num_labels[FAC],FAC) ;
	      
	      
	      if (Soln) {  // if there is a solution, if not, soln=0
		numcol=0;
		temp1=SinkLabels;
		while ((temp1)&&(numcol<minnum)){
		  if (temp1->cond=='C'){  
		    sprintf(vname[*vcnt], "Y_%d_%d", t, PairNum[t]);

		    var_index++;

		    //*****************************************
		    column=(ColumnInfo *)calloc (1,sizeof(ColumnInfo));
		    column->sequence=(int *)calloc (NCust,sizeof(int));
		    column->Col_Time=temp1->labeldata.Time;
		    column->Name=PairNum[t];
		    column->varindex=var_index;
		    //******************************************
		    
		    TotalDemand=0;
		    printf("Y_%d_%d: [", t, PairNum[t]);

		    // Datafield=(LIST_VAR_INDEX *) calloc(1, sizeof(LIST_VAR_INDEX));
		    //Datafield->ind_data.custarray=(int*) calloc(NCust, sizeof(int));
		    //Datafield->ind_data.index= var_index;


		    for (i=0; i < NCust; i++) {
		      if (temp1->labeldata.nodeseq[i]> 0) { 
			//Datafield->ind_data.custarray[i]=temp1->labeldata.nodeseq[i];
			vind[*nzcnt] = i;
			vcoef[(*nzcnt)++] = 1;
			column->sequence[i]=temp1->labeldata.nodeseq[i];

			TotalDemand=TotalDemand+Demand[i]; 
			printf("%d:%d, ",i,temp1->labeldata.nodeseq[i]);
		      } // if 
		    } // for i
		    printf("], C=%lf, T=%d\n",temp1->labeldata.Cost+VFcost-v[FAC], temp1->labeldata.Time);
		    //printf("\n");
		    //fprintf(stderr,"(%d,%d)  ",i,pathArr[i]);
		    
		    // Datafield->next=ListOfVariables[t];
		    //ListOfVariables[t]=Datafield;
		    //Datafield=0;


		    vind[*nzcnt] = NCust+t;
		    vcoef[(*nzcnt)++] = -(TotalDemand);
		    
		    for (i=0;i<NCust;i++) {
		      if (temp1->labeldata.nodeseq[i]>0) {
			vind[*nzcnt] = NCust+NFac+t*NCust+i;
			vcoef[(*nzcnt)++] = -1;
		      }
		    }
		    
		    vind[*nzcnt] = NCust+NFac+NCust*NFac+1+t;
		    vcoef[(*nzcnt)++] = 1;
		    
		    if(ConList){
		      if (LISTofCUST[FAC]) {
			coninfo=LISTofCUST[FAC];
			while (coninfo){
			  if (temp1->labeldata.nodeseq[coninfo->custind]>0) {
			    printf("The variable has non zero coeff in const %d \n", coninfo->conind);
			    vind[*nzcnt] = coninfo->conind;
			    vcoef[(*nzcnt)++] = 1;
			  } //if
			  coninfo=coninfo->next;
			}//while
		      }	//if LISTofCUST[FAC]
		    } //if ConList
		    
		    if (GapConList) {
		      Cut=GapConList;
		      while (Cut){
			p=0;
			for (i=0; i < NCust; i++) {
			  if ((Cut->CustSet[i]>0)&&(temp1->labeldata.nodeseq[i]>0)){
			    if (Cut->fac==FAC) {
			      p=p+Demand[i];
			    }
			    else {
			      p2=FCap-Cut->tdem+Demand[i];
			      if (p2>0)
				p=p+p2;
			    }
			  } //if
			} //for i
			if (p>0) {
			  vind[*nzcnt]=Cut->id;
			  vcoef[(*nzcnt)++]=-p;
			}
			Cut=Cut->next;
		      }//while cut
		    }//if (GapConList) 


		    vobj[*vcnt] =  VOcost/60*temp1->labeldata.Time+VFcost;
		    
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
		
		printf("Number of columns added for facility %d is %d\n",t,numcol);
		fprintf(stdout,"*vcnt: %d\n",*vcnt);
		//fprintf(stderr,"*nzcnt: %d\n",*nzcnt);
		
	      } //if a soln exists   
	      
	      else {
		//fprintf(stdout, "Pricing problem results 0 columns for facility %d \n", t);
		if (SKIPFAC[FAC]==0)
		  SKIPFAC[FAC]=1;
	      }
	      
	    } //if !nonegcol
	    else {
	      // fprintf(stdout, "Skipping pricing problem for Faciality %d\n", t);
	      if (SKIPFAC[FAC]==0)
		SKIPFAC[FAC]=1;
	    }
	  }//if SKIPFAC[t]!=1
	  
	  //	  else 
	    //fprintf(stdout, "SKIPFAC=1, thus, skip pricing problem for Faciality %d\n", t);
	 
	} //if xstat[t] != FORBIDDEN
	//	else //if t=forbidden
	  //  fprintf(stdout, "Skipping pricing problem for Faciality %d, since it is FORBIDDEN\n", t);
      } // for t


      
      if (*vcnt==0) {
	doagain=0;
	for (t=0;t<NFac;t++){
	  if  (SKIPFAC[t]==1){
	    SKIPFAC[t]=2;
	    doagain=1;
	  }
	}
      }
      else
 	doagain=0;
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

    for (i=0; i < NFac; i++)
      free(sigma[i]);
    free(sigma);

    if(ConList){
      for (i=0; i < NFac; i++) {
	if (LISTofCUST[i]) {
	  free(LISTofCUST[i]);
	}
	LISTofCUST[i]=0;
      }
      if (LISTofCUST){
	free(LISTofCUST);
      }
      LISTofCUST=0;
    }


    
  } //IF GENERATE
  
  if (*vcnt!=0){
    fprintf(stdout, "Number of columns found: %d\n", *vcnt);
    
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
  printf("stepcount=%d \n",stepcount);
  /*
  if (stepcount==165){
    sprintf(CurrMpsF,"%s165.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }

  if (stepcount==162){
    sprintf(CurrMpsF,"%s162.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }
  if (stepcount==164){
    sprintf(CurrMpsF,"%s164.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }
  if (stepcount==167){
    sprintf(CurrMpsF,"%s167.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }
  */
  if (stepcount==75){
    sprintf(CurrMpsF,"%s75.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }


  fprintf(stdout,"exiting a_vars.c\n");
  
  return(*vcnt > 0 ? SUCCESS:FAILURE);
  
}
