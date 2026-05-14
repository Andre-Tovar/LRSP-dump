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
extern "C"{
#include "header.h"
#include "minto.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
}

/*
 * appl_divide
 */
/*
int id;               /* identification of active minto 
long depth;           /* identification: depth 
long creation;        /* identification: creation 
double zlp;           /* value of the LP solution 
double *xlp;          /* values of the variables 
double zprimal;       /* value of the primal solution 
double *xprimal;      /* values of the variables 
int *ncnt;            /* variable for number of nodes 
int *vcnt;            /* array for number of variables 
int *vind;            /* array for indices of variables 
char *vclass;         /* array for type of bounds 
double *vvalue;       /* array for value of bounds 
int *nzcnt;           /* variable for number of nonzero coefficients 
int *ccnt;            /* array for number of constraints 
int *cfirst;          /* array for positions of first nonzero coefficients 
int *cind;            /* array for indices of nonzero coefficients 
double *ccoef;        /* array for values of nonzero coefficients 
char *csense;         /* array for senses 
double *crhs;         /* array for right hand sides 
char **cname;         /* array for names 
int bdim;             /* size of bounds arrays 
int sdim;             /* size of small arrays 
int ldim;             /* size of large arrays 
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
unsigned int
appl_divide(int id,
	    long depth,
	    long creation,
	    double zlp,
	    double *xlp,
	    double zprimal,
	    double *xprimal,
	    int *ncnt,
	    int *vcnt,
	    int *vind,
	    char *vclass,
	    double *vvalue,
	    int *nzcnt,
	    int *ccnt,
	    int *cfirst,
	    int *cind,
	    double *ccoef,
	    char *csense,
	    double *crhs,
	    char **cname,
	    int bdim,
	    int sdim,
	    int ldim)
{

  int j; //success;
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
  // CandLIST *tempNod2;
   
  //  FILE *InfoCOL;
  //char Col_file[BUFSIZE];
  //ColumnInfo *column;
  //int i;
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
  // NoTvar=0;
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
    // fprintf(stdout, "No fractional T variables found.\n");
    //    NoTvar=1;
    // fprintf(stdout, "Check for the fractional V_j variables\n");
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
      
      // fprintf(stdout, "No fractional V variables found. Apply Assignment of Cust. to Facilities\n");
      RyanFoster_Rule4=1;
      // RyanFoster=1;
      
    } // if index == -1        // no fractional v_j variable found, branching on Y variables

    else { 
      printf("Branching on V[%d]variable\n", index-NFac);
      
      //num_created++;
      *ncnt = 2;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = TOTALPAIR;
      bbinfo->type = LESS;
      bbinfo->dc = index-NFac;
      bbinfo->r1=(int) floor(xlp[index]);
      bbinfo->r2=-1;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[0] = 0;
      ccnt[0] = 0;
      
      //num_created++;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = TOTALPAIR;
      bbinfo->type = GREATER;
      bbinfo->dc = index-NFac;
      bbinfo->r1=-1;
      bbinfo->r2=(int) floor(xlp[index])+1;
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
    printf("Branching on Facility %d\n", index);
    
    // create two new nodes 
    //num_created++;
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
    
    //num_created++;
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
    if (ApplyRule4==1){
      //Apply rule 4, add constraints for CandFAC and CandCUST
      printf("A candidate facility and customer are found for Branching rule 4\n");
      printf("CandFAC=%d CandCUST=%d \n",CandFAC,CandCUST);
      
      //num_created++;
      
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
      
      
      //num_created++;
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

     
      fprintf(stdout,"selected pair of nodes are: Node1=%d, Node2=%d \n",CandN1,CandN2);
       
      // create two new nodes 
      //num_created++;
      *ncnt = 2;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = PAIR;
      bbinfo->type = CONNECTED;
      bbinfo->n1 = CandN1;
      bbinfo->n2 = CandN2;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[0] = 0;
      ccnt[0] = 0;
      
      //num_created++;
      ndinfo = (NDINFO *) calloc(1,sizeof(NDINFO));
      ndinfo->info = NULL;
      bbinfo = (BBINFO *) calloc(1,sizeof(BBINFO));
      bbinfo->v_type = PAIR;
      bbinfo->type = NOT_CONNECTED;
      bbinfo->n1=CandN1;
      bbinfo->n2=CandN2;
      bbinfo->next = ndinfo->info;
      ndinfo->info = bbinfo;
      ndinfo->parent = nodeinfo[creation];
      nodeinfo[++counter] = ndinfo;
      
      vcnt[1] = 0;
      ccnt[1] = 0;
      
      // exit(1);
      return (SUCCESS);
      

      
    
      
    } //else RyanFoster
  
  } //Ryanfoster_Rule4=1


}

