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


#include "header.h"
/*
 * appl_constraints
 */
 
unsigned
appl_constraints (id, zlp, xlp, zprimal, xprimal,
    nzcnt, ccnt, cfirst, cind, ccoef, csense, crhs, ctype, cname, sdim, ldim)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double *xlp;          /* values of the variables */
double zprimal;       /* value of the primal solution */
double *xprimal;      /* values of the variables */
int *nzcnt;           /* variable for number of nonzero coefficients */
int *ccnt;            /* variable for number of constraints */
int *cfirst;          /* array for positions of first nonzero coefficients */
int *cind;            /* array for indices of nonzero coefficients */
double *ccoef;        /* array for values of nonzero coefficients */
char *csense;         /* array for senses */
double *crhs;         /* array for right hand sides */
int *ctype;           /* array for the constraint types: LOCAL or GLOBAL */
char **cname;         /* array for names */
int sdim;             /* length of small arrays */
int ldim;             /* length of large arrays */
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
  double *ai_Z; 
  int tdemand, success,not_empty,j,k;
  double lhs_cut;
  int x,y,p;
  GAP_CONSTRAINTS *Cut;
  double dFCap;
  int numnewcut;

  if (id) {// if recursive version of minto, no constraints are added
    return (FAILURE);
  }


  if (stepcount==159){
    sprintf(CurrMpsF,"%s159c.mps",inq_file());
    //strcpy(CurrMpsF, "test.mps");
    wrt_prob(CurrMpsF);
  }




  size= NAME_SIZE;
  
  *ccnt=0;
  *nzcnt=0;
  _cstore=0;


  //Generate_GAP_Cuts=0;
  
  if ((counter == 1)||(newnode==FALSE)){
    if (Generate_GAP_Cuts==0)
      return(FAILURE);
    else { 
      printf ("Newnode=FALSE \n");
      printf("Seperation process: \n");
      //SEPERATION FOR GAP INEQUALITIES
      /*
      //Print the solution!
      printf("zlp=%lf \n", zlp);
      for (i=0;i<NFac;i++){
      printf("T[%d]=%lf \n",i,xlp[i]);
      }
      for (i=0;i<NFac;i++){
      printf("V[%d]=%lf \n",i,xlp[NFac+i]);
      }
      for (i=0;i<NFac;i++){
      column=Col_Array[i];
      while (column){
      if (xlp[column->varindex]>0){
      printf("Y_%d_%d : ",i,column->Name);
      printf("%lf: ",xlp[column->varindex] );
      }
      
      for (j=0;j<NCust;j++){
      if (column->sequence[j]){
      if (xlp[column->varindex]>0){
      printf("%d:%d ,",j,column->sequence[j]);
      }
      }
      }//for j
      if (xlp[column->varindex]>0){
      printf("\n");
      }
      column=column->next;
      }
      } //for i
      */
      
      inq_form();
      numberofCONST=info_form.form_ccnt;
      if (numberofCONST > numRows){
	numofNEWCON=0;
	for (i=numRows;i<numberofCONST;i++){
	  inq_constr(i);
	  if (info_constr.constr_status!=DELETED) {
	    numofNEWCON++;
	    printf("Index for non-deleted constraint=%d and name=%s, type =%s\n",i,info_constr.constr_name,info_constr.constr_type == GLOBAL ? "GLOBAL" : "LOCAL");
	  
	    printf("debug: constr index: %d\n", lp_cix(info_constr.constr_name));  
	  }
	  else
	    printf("Index for deleted constraint=%d and name=%s\n",i,info_constr.constr_name);
	} //for i
	numberofCONST=numRows+numofNEWCON;
      }
      
      numnewcut=0;
      ai_Z=(double *) calloc (NCust*NFac,sizeof(double));
      
      for (i=0;i<NFac;i++) {
	if (xlp[i]>= 1 - EPS) {
	  printf("lp_slack(%d)=%lf \n",i,lp_slack(NCust+i));
	  if ((lp_slack(NCust+i)<=EPS)&&(lp_slack(NCust+i)>=-EPS)){ //slack of the capacity constraint is ZERO. Constraint is binding!
	    
	    column=Col_Array[i];
	    not_empty=0; //cust_set is empty?
	    while (column){
	      if (xlp[column->varindex]>EPS) {
		for (j=0;j<NCust;j++) {
		  if (column->sequence[j]>0) {
		    ai_Z[j+NCust*i]= ai_Z[j+NCust*i]+xlp[column->varindex];
		    if (not_empty==0)
		      not_empty=1;
		  }//if j is visited
		} //for
	      }//if
	      column=column->next;
	    } //while
	    
	    /*  
		for (j=0;j<NCust;j++) {
		if( ai_Z[j+NCust*i]>0){
		printf("ai_Z[%d]=%lf, Demand[%d]=%d\n",j,ai_Z[j+NCust*i],j,Demand[j]);
		}
		}
	    */
	    if (not_empty==0){
	      printf("Error in a_cons.c, the customer set is empty! \n");
	      exit(9);
	    }
	    success=0;
	    tdemand=0;
	    lhs_cut=0;
	    for (j=0;j<NCust;j++) {
	      if (ai_Z[j+NCust*i]>0){
		tdemand=tdemand+Demand[j];
		lhs_cut=lhs_cut+Demand[j]*ai_Z[j+NCust*i];
		if ((success==0)&&(ai_Z[j+NCust*i]<1-EPS))
		  success=1;
	      }
	    } //for
	    printf("success=%d, tdemand=%d \n",success,tdemand);
	    if (success) { //means that a customer is served with an other facility
	      
	      for (k=0;k<NFac;k++) {
		if ((k!=i)&&(xlp[k]>EPS)) { //for facility k
		  column=Col_Array[k]; 
		  while (column){
		    if (xlp[column->varindex]>EPS) {
		      x=0;
		      for (j=0;j<NCust;j++) {
			if ((ai_Z[j+NCust*i]>0)&&(column->sequence[j]>0)) {
			  p=FCap-tdemand+Demand[j];
			  if (p>0)
			    x=x+p;		    
			} //if
		      }//for
		      if (x>0)
			lhs_cut= lhs_cut+xlp[column->varindex]*x;
		    }//if
		    column=column->next;
		  } //while
		  
		} //if
	      } //for
	      
	      printf("lhs_cut=%lf >? FCap=%d \n",lhs_cut,FCap);
	      dFCap=FCap;
	      if (lhs_cut > dFCap+EPS) {
		printf("VIOLATED INEQUALITY is found for FAC %d!\n Customer set is [",i);
		for (j=0;j<NCust;j++) {
		  if (ai_Z[j+NCust*i]>0)
		      printf("%d, ",j);
		}
		printf("\n");

		Cut=(GAP_CONSTRAINTS *)calloc (1, sizeof(GAP_CONSTRAINTS));
		Cut->CustSet=(int *)calloc(NCust,sizeof(int));
		Cut->fac=i;
		
		if (_cstore)
		  _cstore=0;
		_cstore = (char *) calloc(size, sizeof(char));
		
		cfirst[*ccnt]=*nzcnt;
		
		column=Col_Array[i];
		while (column){
		  y=0;
		  for (j=0;j<NCust;j++) {
		    if ((ai_Z[j+NCust*i]>0)&&(column->sequence[j]>0)) {
		      Cut->CustSet[j]=1;
		      y=y+Demand[j];
		    } //if had a non zero coeff
		  } //for
		  if (y>0){
		    cind[*nzcnt]=column->varindex;
		    ccoef[(*nzcnt)++]=-y;
		  }
		  column=column->next;
		}//while column
		
		for (k=0;k<NFac;k++) {
		  if (k!=i) { //for facility k
		    column=Col_Array[k];
		    while (column){
		      x=0;
		      for (j=0;j<NCust;j++) {
			if ((ai_Z[j+NCust*i]>0)&&(column->sequence[j]>0)) {
			  p=FCap-tdemand+Demand[j];
			  if (p>0)
			    x=x+p;
			} //if had a non zero coeff
		      }
		      if (x>0){
			cind[*nzcnt]=column->varindex;
			//printf("index=%d \n",column->varindex);
			ccoef[(*nzcnt)++]=-x;
		      }
		      column=column->next;
		    } //while
		  } //if
		  
		} //for
		
		crhs[*ccnt] = (double) -FCap;
		csense[*ccnt] = 'G';
		sprintf(&(_cstore[0]),"GAP_%d",num_gap_cuts);
		_cstore[size - 1] = '\0';
		cname[*ccnt] = _cstore;
		ctype[(*ccnt)++]=GLOBAL;
	
		//Cut->id=numberofCONST;
		Cut->id=numRows+num_gap_cuts;
		//	Cut->id=numberofCONST;
		Cut->order=num_gap_cuts;

		Cut->tdem=tdemand;
		num_gap_cuts++;
		numberofCONST++;
		Cut->next=GapConList;
		GapConList=Cut;
		Cut=0;
		numnewcut++;
	      } //if
	    } //if success
	  } //if const is binding
	} //if T[i]==1
      } //for all i 
      
      if (ai_Z) {
	free(ai_Z);
	ai_Z=0;
      } //if

      if ((numnewcut>0)&&(ConList)){
	printf("A new GAP is added in addition to the branching constraints, numnewcut=%d \n",numnewcut);
	addcon=ConList;
	while (addcon) {
	  printf("Previous index=%d for fac=%d, cust=%d \n",addcon->index, addcon->facility, addcon->customer);
	  addcon->index=addcon->index + numnewcut;
	  printf("Later index=%d \n",addcon->index);
	  addcon=addcon->next;
	}
      }

      if (*ccnt>0) {
	cfirst[*ccnt]=*nzcnt;
	return(SUCCESS);
      }
      else 
	return (FAILURE);
  
    } //else
      //      cfirst[*ccnt]=*nzcnt;
  }//if counter or newnode==false
  

  
  if (newnode == TRUE) {
   
    newnode = FALSE;
    
    if (!ConList) 
      return(FAILURE);
   

    /*
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
      // printf("constraint with index %d is DELETED \n",i);
    } //for i
    
    if (numofNEWCON!=0){
      
      printf("numberof constraints in current model=%d, number of rows in original=%d, number of non-deleted=%d \n ",numberofCONST,numRows,numofNEWCON);
      printf("ERROR, the previous constraints are not deleted \n");
      exit(1);
    }
    */

    //add ALL constraints from ConList
    
    addcon=ConList;
    
    // size= NAME_SIZE*nnewcon;
    //printf("nnewcon=%d \n",nnewcon);
    
    inq_form();
    numberofCONST=info_form.form_ccnt;
    if (numberofCONST > numRows){
      numofNEWCON=0;
      for (i=numRows;i<numberofCONST;i++){
	inq_constr(i);
	if (info_constr.constr_status!=DELETED) {
	  numofNEWCON++;
	  printf("Index for non-deleted constraint=%d and name=%s\n",i,info_constr.constr_name);
	}
	else
	  printf("Index for deleted constraint=%d and name=%s\n",i,info_constr.constr_name);
      } //for i

      numberofCONST=numRows+numofNEWCON;
    }

    _cstore=0;
    //_cstore = (char *) calloc(size, sizeof(char));
    
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
	  ccoef[(*nzcnt)++]=1.0;
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

      addcon->index=numberofCONST;
      numberofCONST++;
      addcon=addcon->next;
    } //while addcon
    
    cfirst[*ccnt]=*nzcnt;
    
    //  if (_cstore) 
    //free(_cstore);
    //_cstore=0;
    
    return (SUCCESS);
  } //else
  
  
}



