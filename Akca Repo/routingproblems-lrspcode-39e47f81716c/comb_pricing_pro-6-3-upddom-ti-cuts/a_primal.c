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
 * A_PRIMAL.C
 */


#include "header.h"

/*
 * appl_primal
 */

typedef struct NZVar{
  int index; //index stored in minto
  int fac; //facility
  int pairno; //pair no in names
  int *seq;
  struct NZVar *next;
} nzVAR ;

/*typedef struct CandidateList {
  int n1;
  int n2;
  double frac;
  struct CandidateList *next;
} CandLIST;
*/

unsigned
appl_primal (id, zlp, xlp, intlp, zprimal, xprimal, zpnew, xpnew, xpstat)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double *xlp;          /* values of the variables */
int intlp;            /* integer indicating whether LP solution is integral */
double zprimal;       /* value of the primal solution */
double *xprimal;      /* values of the variables */
double *zpnew;        /* variable for new value of the primal solution */
double *xpnew;        /* array for new values of the variables */
int *xpstat;          /* variable to indicate existence of solution vector */
{


  //***********************
  //what does counter do??
  //**************************

  int j,i,find,d,facility,pairNO,success,k,found,thereisfractional;
  // my_var *temp;
  // FILE *fp_log;
  double frac;
  int thereisbranching;
  nzVAR *varindex,*temp,*temp1; 
  char *s;
  ColumnInfo *column;
  CandLIST  *tempNod,*tempNod2;
  double tcost;
  int mark;
  int *CustAssignArray;



  fprintf(stdout,"inside a_primal,before id and counter check\n");
  fprintf(stdout,"id:%d, counter:%d \n",id,counter);
 
  ApplyRule4=0;
 
  if (id)
    return(FAILURE);
  
  /*
  if (id){
    if (!setupperbnd){
      *zpnew =-heur_upper_bnd;
      *xpstat = TRUE;
      setupperbnd=1;
      return (SUCCESS);
    }
    else 
      return(FAILURE);
  }
  */


  if (!((counter > 1 && newnode) || (counter==1))){
    printf("Checking for a possible integer solution\n");
  
    inq_form();
    //check for fractional facility
  
    thereisbranching=0;

    tempNod2=NodeList;
    while (tempNod2){
      tempNod=tempNod2->next;
      tempNod2->next=0;
      free(tempNod2);
      tempNod2=tempNod;    
    }
    //free(NodeList);
    NodeList=0;
    
    /*
      temp=varindex;
      while(temp){
      temp1=temp->next;
      //temp->next=0;
      free(temp->seq);
      free(temp);
      temp=temp1;
      } 
    */
  

    for (j=0; j < NFac; j++) { //look at T variables
      inq_var(j, NO);    // retrieves info about jth variable without
      frac = xlp[j] - floor(xlp[j]);
      if (frac > EPS && frac < 1 - EPS) {
	thereisbranching=1;
	break;
      } // if frac > EPS ...
    } // for j

    if (!thereisbranching) {
      for (j=NFac; j < 2*NFac; j++) { //look for v variables
	inq_var(j, NO);    // retrieves info about jth variable without
	frac = xlp[j] - floor(xlp[j]);
	if (frac > EPS && frac < 1 - EPS) {
	  thereisbranching=1;
	  break;
	} // if frac > EPS ...
      }//for j

      if (!thereisbranching) {

	//first let's search for Branching rule 4:
	CustAssignArray=(int *)calloc (NCust,sizeof(int));
	for(j=0;j<NCust;j++)
	  CustAssignArray[j]=-1;

	found=0; //will show us weather CandFAC, and CandCUST is found or not
	thereisfractional=0;
	printf("Searching for rule 4 \n");
	for (j=2*NFac+NCust+1; j <info_form.form_vcnt; j++) {
	  frac = xlp[j] - floor(xlp[j]);
	  if (frac > EPS && frac < 1 - EPS) { //means there is a fractional variable	  
	    if (thereisfractional==0)
	      thereisfractional=1;

	    inq_var(j, YES);
	    
	    s= (char*) calloc (NAME_SIZE,sizeof(char)); 
	    strcpy(s,info_var.var_name);
	    d=0;  
	    facility=-1;
	    d=getPairNum(s,&facility,&pairNO); //from the name of variable, gets facility and the number of pair
	    free(s);
	    if (facility!=-1){
	      for (i=0; i <info_var.var_nz; i++) {
		if (info_var.var_ind[i]<NCust){
		  if (CustAssignArray[info_var.var_ind[i]]==-1)
		    CustAssignArray[info_var.var_ind[i]]=facility;
		  else if (CustAssignArray[info_var.var_ind[i]]!=facility) {
		    CandFAC=facility;
		    CandCUST=info_var.var_ind[i];
		    found=1;
		    break;
		  }
		} //if <NCust
	      } //for
	    }
	    else {
	      printf("ERROR: in reading the facility number from the name of the variable \n");
	      exit(1);
	    }
	    if (found==1)
	      break;
	  } //if (frac > EPS && frac < 1 - EPS) 
	} //for j

	free(CustAssignArray);
	CustAssignArray=0;

	if (found==1) {
	  printf("A candidate pair of cust and fac is found \n");
	  thereisbranching=1;
	  ApplyRule4=1;
	}
	
	else {
	  if (thereisfractional==0){
	    printf("No fractional variable is found. The LP solution = IP solution\n");
	    return(FAILURE);
	  }
	  else {
	    printf("Searching for Ryan and Foster \n");
	    //Look for Ryan and Foster rule
	    varindex=0;
	    for (j=2*NFac+NCust+1; j <info_form.form_vcnt; j++) {
	      inq_var(j, NO);    // retrieves info about jth variable without
	      frac = xlp[j] - floor(xlp[j]);
	      if (frac > EPS && frac < 1 - EPS) { //means there is a fractional variable
		//store the information about non-zero variable
		temp=(nzVAR *) calloc (1,sizeof(nzVAR));
		temp->index=j;
		inq_var(j,NO);
		s= (char*) calloc (NAME_SIZE,sizeof(char)); 
		strcpy(s,info_var.var_name);
		d=0;   
		d=getPairNum(s,&facility,&pairNO); //from the name of variable, gets facility and the number of pair
		free(s);
		temp->fac=facility;
		temp->pairno=pairNO;
		temp->seq=(int *)calloc (2*NCust+1,sizeof(int)); //keeps the sequence of nodes in the pairing
		temp->seq[0]=source;
		column=Col_Array[facility];
		success=0;
		while (column){
		  if (column->Name==pairNO){
		    success=1;
		    
		    for (k=1;k<2*NCust+1;k++){
		      find=0;
		      for (d=0;d<NCust;d++){	    
			if (column->sequence[d]==k){
			  find=1;
			  temp->seq[k]=d; 
			  break;
			}
		      }//for d
		      
		      if (!find){
			if ((k>1)&& (temp->seq[k-1]==sink)){
			  temp->seq[k]=-1;
			  break;
			}
			temp->seq[k]=sink;
		      } 
		    }//for k
		    
		  }//if column->name=pairNO
		  if (success)
		    break;
		  column=column->next;
		} //while
		
		if (!success) {
		  printf("ERROR: cannot find the searched pair in pair array \n");
		  exit(1);
		}      
		
		temp->next=0;
		temp->next=varindex;
		varindex=temp;
		temp=0;
	      } // if frac > EPS ...
	    } //j
	  
	    /* Print the fractional variables*/
	    
	    temp=varindex;
	    printf("Fractional Y variables:\n");
	    while (temp){
	      printf("[%lf ,Y%d_%d, seq=",xlp[temp->index],temp->fac,temp->pairno);
	      for (j=0;j<2*NCust+1;j++){
		printf("%d ",temp->seq[j]);
		if (temp->seq[j]<0)
		  break;
	      }
	      printf("]\n");
	      temp=temp->next;    
	    } //while
	    
	    success=0;
	    temp=varindex;
	    //form a list for fractional node pairs 
	    while (temp){
	      for (j=0;j<2*NCust;j++){
		if (temp->seq[j]!=-1) {
		  if (temp->seq[j]<NCust) {
		    if (temp->seq[j+1]!=-1){
		      if (temp->seq[j+1]<NCust) {
			if(!NodeList){
			  tempNod=(CandLIST *)calloc (1,sizeof(CandLIST));
			  tempNod->n1=temp->seq[j];
			  tempNod->n2=temp->seq[j+1];
			  tempNod->frac=xlp[temp->index];
			  tempNod->next=NodeList;
			  NodeList=tempNod;
			} //list is empty
			else{
			  tempNod2=NodeList;
			  find=0;
			  while(tempNod2){
			    if((tempNod2->n1==temp->seq[j])&&(tempNod2->n2==temp->seq[j+1])) {
			      find=1;
			      tempNod2->frac=tempNod2->frac+xlp[temp->index];
			      break;
			    }
			    tempNod2=tempNod2->next;
			  }//while
			  if (!find){
			    tempNod=(CandLIST *)calloc (1,sizeof(CandLIST));
			    tempNod->n1=temp->seq[j];
			    tempNod->n2=temp->seq[j+1];
			    tempNod->frac=xlp[temp->index];
			    tempNod->next=NodeList;
			    NodeList=tempNod;
			  } //if not find
			} //else not empty
		      }// j+1 <NCust
		    } //if j+1 not -1
		    else
		      break;
		  } //j < NCust
		}//if j not -1
		else
		  break;
	      }//for all j
	      temp=temp->next;
	    } //while temp
	    
	    tempNod2=NodeList;
	    success=0;
	    while (tempNod2){
	      if (tempNod2->frac<=1 - EPS) {
		success=1;
		break;
	      } //if
	      tempNod2=tempNod2->next;
	    }//while
	    
	    if (!success) {
	      if (!varindex){
		printf("ERROR: thereisfractional=0 but it enters to find pairs for R-F rule.\n");
		return(FAILURE);
	      }

	      printf("No Ryan and Foster pair and No candidate facility and customer could be found for Branching\n");
	      printf("Each customer is served by one facility, thus LP solution = IP solution\n");
	    	  
	      *zpnew = zlp;
	      *xpstat = TRUE;
	      for (j = 0; j < info_form.form_vcnt; j++) {
		//if (xlp[j]>EPS)
		xpnew[j]=xlp[j];  
	      }//for
	      return (SUCCESS);
	    } //if //(!success)
	  } //else


    
	 
	  //FREE VARINDEX 
	  //Find a customer i and facility j to write a constraint
	  
	  
	} //else
      }//if !thereisbranching
    }//if !thereisbranching
  } //if not
  
  
  
  if (id == 0 && counter == 1) {

    //no branch and bound, just default primal heuristic
    //return(FAILURE);
 
   /*
    //heuristic UPPER BOUND    
    tcost=heurmain2(dist);
    printf("cost of heuristic solution =%lf  \n",tcost);
   
    *zpnew =-tcost;
    *xpstat = TRUE;
    return(SUCCESS);
    */
    ////////////////////////////////////////////
    *zpnew =-heur_upper_bnd;
    *xpstat = TRUE;
    return(SUCCESS);
    //exit(1);
    //return (FAILURE);
    
    printf("currMpsFile=%s \n",CurrMpsF);
    minto(CurrMpsF,"-o2 -t7200");
    //minto("test.mps","-o2 -p0 ");             //"-p0 -e0 -E0 -n3 -o2 -m1");
    fprintf(stdout,"BB is done for the root node \n");
    *zpnew = -info_opt.opt_value;
    printf ("Optimal value: %f\n", info_opt.opt_value);
    *xpstat = TRUE;
       
    for (j = 0; j < info_form.form_vcnt; j++) 
      xpnew[j]=0;

    for (j=0; j < info_opt.opt_nzcnt; j++){
      xpnew[info_opt.opt_ix[j]] = info_opt.opt_val[j];
    } //for
  
    
  }
  
  if (*xpstat)
    return (SUCCESS);
  else
    return (FAILURE);
    
  //return (FAILURE);
}
