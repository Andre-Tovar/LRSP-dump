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
 *     (C)opyright 1992-1996 - M.W.P. Savelsbergh
 */

/*
 * A_NODE.C
 */


#include "header.h"
/*
 * appl_node
 */

unsigned
appl_node (id, depth, creation, zprimal, xprimal)
int id;               /* identification of active minto */
long depth;           /* identification: depth */
long creation;        /* identification: creation */
double zprimal;       /* value of primal solution */
double *xprimal;      /* values of the variables */
{
  int j,n1,n2,fac,pairNO,d,n1n2,i;
  NDINFO *ndinfo;
  BBINFO *bbinfo;
  double gap;
  int numrules=0;
  BranchDec *decision,*decision1;
  Pair_VAR *temp,*temp1;
  char *s;
  CONSTRAINTS *addcon, *addcon1;
   
  if (id) // if recursive version of MINTO, don't set variable status
    return (CONTINUE);

  /*  if (NoTvar){
    //  newnode=TRUE;
    return (CONTINUE);
  }
  */

  if (!xstat)
    xstat = (char *) calloc(NFac, sizeof(char));

  if (!xstat_l)
    xstat_l = (char *) calloc(NFac, sizeof(char));
  if (!xstat_g)
    xstat_g = (char *) calloc(NFac, sizeof(char));

  if (!x_rhs_l)
    x_rhs_l= (int *) calloc(NFac, sizeof(int));
  
  if (!x_rhs_g)
    x_rhs_g= (int *) calloc(NFac, sizeof(int));

  if (!max_num_labels)
    max_num_labels=(int*)calloc(NFac,sizeof(int));
  if (!DoCWHeurColumnGen)
    DoCWHeurColumnGen=(int*)calloc(NFac,sizeof(int));
  if(!SKIPFAC)
    SKIPFAC=(int*)calloc(NFac,sizeof(int));


  for (j=0; j < NFac; j++){
    xstat[j] = FREE;
    xstat_g[j] =FREE;
    xstat_l[j]=FREE; 
  }
  
  temp=PairBounds;
  while(temp){
    temp1=temp->next;
    free(temp);
    temp=temp1;

  }
  PairBounds=0;

  decision=Rules;
  while (decision) {
    decision1=decision->next;
    free(decision);
    decision =decision1;
  }
  Rules =0;
 
  addcon=ConList;
  while (addcon) {
    addcon1=addcon->next;
    free(addcon);
    addcon =addcon1;
  }
  ConList =0;
  nnewcon=0;





  if (creation > 1) {
    for (ndinfo = nodeinfo[creation]; ndinfo; ndinfo = ndinfo->parent) {
      for (bbinfo = ndinfo->info; bbinfo; bbinfo = bbinfo->next) {
	if (bbinfo->type == REQUIRED) 
	  xstat[bbinfo->dc] = REQUIRED;
	
        if (bbinfo->type == FORBIDDEN)
          xstat[bbinfo->dc] = FORBIDDEN;
	
	if (bbinfo->type ==LESS) {
	  if (xstat_l[bbinfo->dc]==LESS){ //if already there is a <= constraint, take the smallest rhs
	    if (x_rhs_l[bbinfo->dc]> bbinfo->r1)
	      x_rhs_l[bbinfo->dc]=bbinfo->r1;
	  }
	  else {
	    xstat_l[bbinfo->dc] =LESS;
	    x_rhs_l[bbinfo->dc]=bbinfo->r1;
	  }
	} //if LESS
	  
	if (bbinfo->type ==GREATER){ 
	  if (xstat_g[bbinfo->dc]==GREATER){
	    if (x_rhs_g[bbinfo->dc]< bbinfo->r2)
	      x_rhs_g[bbinfo->dc]=bbinfo->r2;
	  }
	  else {
	    xstat_g[bbinfo->dc] =GREATER;
	    x_rhs_g[bbinfo->dc]=bbinfo->r2;
	  }
	  
	} //if GREATER

	if (bbinfo->type == CONNECTED){
          
	  fprintf(stdout, "In loop for CONNECTED\n");
	  //exit(1);
	  //mark indices of variables in which only Path1 or Path2 is present
	  inq_form();
	  n1=bbinfo->n1;
	  n2=bbinfo->n2;

	  //fill the information about Y branching
	  decision=(BranchDec *) calloc (1,sizeof(BranchDec));
	  decision->N1=n1;
	  decision->N2=n2;
	  decision->type=CONNECTED;
	  decision->next=0;
	
	  if (!Rules) {
	    Rules=decision;
	    decision=0;
	  }
	  
	  else {
	    decision->next=Rules;
	    Rules=decision;
	    decision=0;
	  }
	  
	  /////////////////////////////////////

	  fprintf(stdout,"Node1=%d, Node2=%d\n",n1,n2);

	  for (j=2*NFac+NCust+1; j <info_form.form_vcnt; j++) {
	    inq_var(j,NO);
	    
	    s= (char*) calloc (NAME_SIZE,sizeof(char)); 
	    strcpy(s,info_var.var_name);
	    d=0;   
	    d=getPairNum(s,&fac,&pairNO); 
	    free(s);
	    d=0;
	 

	    n1n2=1;
	    d=search_for_nodes (Col_Array[fac],pairNO,n1,n2,n1n2);

	   
	    if (d==1) { //get index since column should be marked for zero..
	      temp=(Pair_VAR*)calloc (1,sizeof(Pair_VAR));
	      temp->index=j;
	      temp->next=PairBounds;
	      PairBounds=temp;
	    }
	  }//for j
	  
	  /* 
	  temp=PairBounds;
	  while (temp){
	    printf("index of variable= %d \n",temp->index);
	    temp=temp->next;
	  } //
	  */
	} //CONNECTED 

	if (bbinfo->type ==NOT_CONNECTED){
        
	  fprintf(stdout, "In loop for NOT_CONNECTED\n");
	  
	  //mark indices of variables in which only Path1 or Path2 is present
	  inq_form();
	  n1= bbinfo->n1;
	  n2=bbinfo->n2;
	
	  //fill the information about Y branching
	  decision=(BranchDec *) calloc (1,sizeof(BranchDec));
	  decision->N1=n1;
	  decision->N2=n2;
	  decision->type=NOT_CONNECTED;
	  decision->next=0;
	  if (!Rules) {
	    Rules=decision;
	    decision=0;
	  }
	  else {
	    decision->next=Rules;
	    Rules=decision;
	    decision=0;
	  }
	  /////////////////////////////////////

	  fprintf(stdout,"Node1=%d, Node2=%d\n",n1,n2);
	  
	  for (j=2*NFac+NCust+1; j <info_form.form_vcnt; j++) {
	    inq_var(j,NO);
	    
	    s= (char*) calloc (NAME_SIZE,sizeof(char)); 
	    strcpy(s,info_var.var_name);
	    d=0;   
	    d=getPairNum(s,&fac,&pairNO); 
	    free(s);
	    d=0;
	    
	    n1n2=0;
	    
	    d=search_for_nodes (Col_Array[fac],pairNO,n1,n2,n1n2);

	    if (d==1) { //get index since column should be marked for zero..
	      temp=(Pair_VAR*)calloc (1,sizeof(Pair_VAR));
	      temp->index=j;
	      temp->next=PairBounds;
	      PairBounds=temp;
	    }
	  }//for j
	  
	  /*
	  temp=PairBounds;
	  while (temp){
	    printf("index of variable= %d \n",temp->index);
	    temp=temp->next;
	  } //
	  */

	} //NOT_CONNECTED 

	if (bbinfo->type ==LESSCON){
	  nnewcon++;
        
	  fprintf(stdout, "Write a LESS Constraint \n");
	 
	  addcon=(CONSTRAINTS *) calloc (1,sizeof(CONSTRAINTS));
	  addcon->facility= bbinfo->n1;
	  addcon->customer= bbinfo->n2;
	  addcon->type= LESSCON;
	  addcon->next=0;
	 
	  fprintf(stdout, "CandFAC=%d , CandCUST=%d \n", addcon->facility,addcon->customer);
	  
	  if (!ConList) {
	    ConList=addcon;
	    addcon=0;
	  }
	  else {
	    addcon->next=ConList;
	    ConList=addcon;
	    addcon=0;
	  }
	  /////////////////////////////////////
	  
	  

	} //=LESSCON 
	
	if (bbinfo->type ==GREATERCON){

	  nnewcon++;

	  fprintf(stdout, "Write a GREATER Constraint \n");
	    
	  addcon=(CONSTRAINTS *) calloc (1,sizeof(CONSTRAINTS));
	  addcon->facility= bbinfo->n1;
	  addcon->customer= bbinfo->n2;
	  addcon->type= GREATERCON;
	  addcon->next=0;
	  

	  fprintf(stdout, "CandFAC=%d , CandCUST=%d \n", addcon->facility,addcon->customer);

	  if (!ConList) {
	    ConList=addcon;
	    addcon=0;
	  }
	  else {
	    addcon->next=ConList;
	    ConList=addcon;
	    addcon=0;
	  }
	  /////////////////////////////////////
	  
	  

	} //=GREATERCON 
	

	
      } // for bbinfo
    } // for ndinfo
  } // if
  
  newnode = TRUE;
  
  // SetThreshold=(int*)calloc(NFac,sizeof(int));
  //  for (i=0;i<NFac;i++)
  //SetThreshold[i]=1;

  //max_num_labels=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
     max_num_labels[i]=200;


 
  //DoCWHeurColumnGen=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
    DoCWHeurColumnGen[i]=1;

  //SKIPFAC=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
    SKIPFAC[i]=0;




  // gap=stat_gap();
  //fprintf(stderr,"integrality gap=%lf \n",gap);
  return (CONTINUE);
}
