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
 * A_DIVIDE.C
 */

#include "header.h"
#include <string.h>
/*
 * appl_divide
 */

/*
typedef struct NZVar{
  int index; //index stored in minto
  int fac; //facility
  int pairno; //pair no in names
  int *seq;
  struct NZVar *next;
} nzVAR ;
typedef struct CandidateList {
  int n1;
  int n2;
  double frac;
  struct CandidateList *next;
} CandLIST;
*/

unsigned
appl_divide (id, depth, creation, zlp, xlp, zprimal, xprimal,
    ncnt, vcnt, vind, vclass, vvalue,
    nzcnt, ccnt, cfirst, cind, ccoef, csense, crhs, cname,
    bdim, sdim, ldim)
int id;               /* identification of active minto */
long depth;           /* identification: depth */
long creation;        /* identification: creation */
double zlp;           /* value of the LP solution */
double *xlp;          /* values of the variables */
double zprimal;       /* value of the primal solution */
double *xprimal;      /* values of the variables */
int *ncnt;            /* variable for number of nodes */
int *vcnt;            /* array for number of variables */
int *vind;            /* array for indices of variables */
char *vclass;         /* array for type of bounds */
double *vvalue;       /* array for value of bounds */
int *nzcnt;           /* variable for number of nonzero coefficients */
int *ccnt;            /* array for number of constraints */
int *cfirst;          /* array for positions of first nonzero coefficients */
int *cind;            /* array for indices of nonzero coefficients */
double *ccoef;        /* array for values of nonzero coefficients */
char *csense;         /* array for senses */
double *crhs;         /* array for right hand sides */
char **cname;         /* array for names */
int bdim;             /* size of bounds arrays */
int sdim;             /* size of small arrays */
int ldim;             /* size of large arrays */
{

  int j,success;
    //k,find,d,facility,pairNO,
  int index = -1;
  double frac;
  double diff;
  double mindiff = (double) 1;
  int RyanFoster, RyanFoster_Rule4;
  NDINFO *ndinfo;
  BBINFO *bbinfo;
  //  nzVAR *var_index,*temp,*temp1; 
  // char *s;
  //ColumnInfo *column;
  // double Totalfrac, tempfrac;
  //int sink_Node1, sink_Node2,source_Node1,source_Node2;
  //CandLIST  *tempNod;
  CandLIST *tempNod2;
   
  FILE *InfoCOL;
  char Col_file[BUFSIZE];
  ColumnInfo *column;
  int i;
  //char ConName[BUFSIZE];
  // char *s;
  // int d,fac,pairNO;



  // default branching scheme implemented for recursive version of minto
  if (id) 
    return (FAILURE);

 
  inq_form();   // initializes info_form; retrieves #vars, #constraints

  /*
  
  tempNod2=NodeList;
  while (tempNod2){
    tempNod=tempNod2->next;
    // tempNod2->next=0;
    free(tempNod2);
    tempNod2=tempNod;    
  }
  free(NodeList);
  */
  /*
  temp=var_index;
  while(temp){
    temp1=temp->next;
    //temp->next=0;
    free(temp->seq);
    free(temp);
    temp=temp1;
  } 
  */
  
  //var_index=0;
  // NodeList=0;  
    

  // Branching strategy 1: branch on most fractional T(location) variable
  NoTvar=0;
  RyanFoster=0;
  RyanFoster_Rule4=0;

  for (j=0; j < NFac; j++) {
    inq_var(j, NO);    // retrieves info about jth variable without
                       // retrieving column
    frac = xlp[j] - floor(xlp[j]);
    if (frac > EPS && frac < 1 - EPS) {
      diff = fabs(frac - 0.5);
      if (diff < mindiff) {
	mindiff = diff;
        index = j;
      } // if diff < mindiff
    } // if frac > EPS ...
  } // for j
  
  if (index == -1) {
    //no fractional T variable
    fprintf(stdout, "No fractional T variables found.\n");
    NoTvar=1;
    fprintf(stdout, "Check for the fractional V_j variables\n");
    mindiff=1;
    for (j=NFac; j < 2*NFac; j++) {
      inq_var(j, NO);    // retrieves info about jth variable without
                       // retrieving column
      frac = xlp[j] - floor(xlp[j]);
      if (frac > EPS && frac < 1 - EPS) {
	diff = fabs(frac - 0.5);
	if (diff < mindiff) {
	  mindiff = diff;
	  index = j;
	} // if diff < mindiff
      } // if frac > EPS ...
    }//for j

    if (index == -1) { //branching on Rule 4 or Ryan and Foster
      
      fprintf(stdout, "No fractional V variables found. Do not generate new nodes.\n");
      RyanFoster_Rule4=1;
      // RyanFoster=1;
      
    } // if index == -1        // no fractional v_j variable found, branching on Y variables

    else { 
      fprintf(stdout, "Branching on V[%d]variable\n", index-NFac);
      
      num_created++;
      *ncnt = 2;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = TOTALPAIR;
      bbinfo->type = LESS;
      bbinfo->dc = index-NFac;
      bbinfo->r1=floor(xlp[index]);
      bbinfo->r2=-1;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[0] = 0;
      ccnt[0] = 0;
      
      num_created++;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = TOTALPAIR;
      bbinfo->type = GREATER;
      bbinfo->dc = index-NFac;
      bbinfo->r1=-1;
      bbinfo->r2=floor(xlp[index])+1;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[1] = 0;
      ccnt[1] = 0;
      
      return (SUCCESS);
    } //else
  } //if
  else {
    fprintf(stdout, "Branching on Facility %d\n", index);
    
    // create two new nodes 
    num_created++;
    *ncnt = 2;
    ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
    ndinfo->info = NULL;
    bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
    bbinfo->v_type = LOCATION;
    bbinfo->type = REQUIRED;
    bbinfo->dc = index;
    bbinfo->next = ndinfo->info;
    ndinfo->info = bbinfo;
    ndinfo->parent = nodeinfo[creation];
    nodeinfo[++counter] = ndinfo;
    
    vcnt[0] = 0;
    ccnt[0] = 0;
    
    num_created++;
    ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
    ndinfo->info = NULL;
    bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
    bbinfo->v_type = LOCATION;
    bbinfo->type = FORBIDDEN;
    bbinfo->dc = index;
    bbinfo->next = ndinfo->info;
    ndinfo->info = bbinfo;
    ndinfo->parent = nodeinfo[creation];
    nodeinfo[++counter] = ndinfo;
    
    vcnt[1] = 0;
    ccnt[1] = 0;
    
    return (SUCCESS);
  } //else =branching on T


  if (RyanFoster_Rule4==1) {
    //means do not generate new nodes return SUCCESS immediately
    *ncnt = 0;
    return(SUCCESS);

    //The rest would not be run because we don't do rule 4 and Ryan and Foster

    if (ApplyRule4==1){
      //Apply rule 4, add constraints for CandFAC and CandCUST
      printf("A candidate facility and customer are found for Branching rule 4\n");
      printf("CandFAC=%d CandCUST=%d \n",CandFAC,CandCUST);
      
      num_created++;
      
      *ncnt = 2;
      
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = CONS;  //means we add a constraint
      bbinfo->type = LESSCON;
      bbinfo->n1 = CandFAC;  //facility to branch on
      bbinfo->n2 = CandCUST;   //customer to branch on
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[0] = 0;
      
      
      num_created++;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = CONS;
      bbinfo->type = GREATERCON;
      bbinfo->n1=CandFAC;
      bbinfo->n2=CandCUST;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[1] = 0;
      
      
      ccnt[0] = 0;
      ccnt[1] = 0;
      return(SUCCESS);
    }
    
    else{
      //Apply modified ryan and foster Branching rule
      printf("ApplyRule4=%d \n",ApplyRule4);
      //var_index=0;
     
      /* Print the fractional variables*/
      /*
	temp=var_index;
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
	}
      */
      /*****************************************/
      
      //Now we have the linked list keeping all fractional 
      //variables and the sequence of nodes in these variables..
      
      //Now find the link to be branch on...
      /***************************************************************************/
   
      /* Printing the candidate node pairs list */
      
      tempNod2=NodeList;
      printf("Candidate Node pairs:\n");
      while (tempNod2){
	printf("[Node1=%d ,Node2=%d, frac=%lf \n",tempNod2->n1,tempNod2->n2,tempNod2->frac);
	
	tempNod2=tempNod2->next;    
      }
      
      //take the first candidate customer nodes
      //if not choose an arc from a custmer to a sink 
      // or sink to a customer
      //  or an arc from source to customer
      
      tempNod2=NodeList;
      success=0;
      Node1=-1;
      Node2=-1;
      
     
      while (tempNod2){
	if (tempNod2->frac<=1 - EPS) {
	  if ((tempNod2->n1<NCust)&&(tempNod2->n2<NCust)){
	    Node1=tempNod2->n1;
	    Node2=tempNod2->n2;
	    success=1;
	    break;
	  } //if
	} //if frac
	
	tempNod2=tempNod2->next;
      }//while
      
      if (Node1<0) {
	printf("no Ryan and foster for customer nodes \n");
	printf("ERROR: This line should not be run\n");
	
	
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
	
	exit(9);
      }
      
      fprintf(stdout,"selected pair of nodes are: Node1=%d, Node2=%d \n",Node1,Node2);
      
      
      //clean the memory //////////////////////
      /*
	tempNod2=NodeList;
	while (tempNod2){
	tempNod=tempNod2->next;
	tempNod2->next=0;
	free(tempNod2);
	tempNod2=tempNod;    
	}
	free(NodeList);
	
	
	temp=var_index;
	while(temp){
	temp1=temp->next;
	temp->next=0;
	free(temp->seq);
	free(temp);
	temp=temp1;
	}
	free(var_index);
      */
      //////////////////////////////////////
    
    
      fprintf(stdout, "Ryan and Foster Branching: Nodes are %d and %d\n",Node1,Node2);
      
      // create two new nodes 
      num_created++;
      *ncnt = 2;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = PAIR;
      bbinfo->type = CONNECTED;
      bbinfo->n1 = Node1;
      bbinfo->n2 = Node2;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[0] = 0;
      ccnt[0] = 0;
      
      num_created++;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = PAIR;
      bbinfo->type = NOT_CONNECTED;
      bbinfo->n1=Node1;
      bbinfo->n2=Node2;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[1] = 0;
      ccnt[1] = 0;
      
      // exit(1);
      return (SUCCESS);
      

      
      

 
    ///******************************************************************// I 
    /* I MIGHT NEED THIS PART */
    /* do not do this time, first see the test results..
       decision=Rules;
       while (decision){
       if ((decision->P1==Path1)&&(decision->P2==Path2)) {
       fprintf(stderr,"Same branching pair is found, find another branching pair!\n");
       success=0;
       break;
       }
       decision=decision->next;
       } //while
    */
    //*********************************************************************//
      
      
    } //else RyanFoster
  
  } //Ryanfoster_Rule4=1


}

