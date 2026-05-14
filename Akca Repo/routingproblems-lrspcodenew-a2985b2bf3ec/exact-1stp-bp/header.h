#ifndef HEADER_H
#define HEADER_H

//change structure for the domination rule..
//add prev, nodeseq..

//#include <stdio.h>
//#include <stdlib.h>
//#include <math.h>
//#include <string.h>
//#include "minto.h"

#include <time.h>

#define BUFSIZE 64
#define NAME_SIZE 16
#define MY_INF 1000000000
//#define NUMNODES 500
//#define TIMELIMIT 150
//#define MAXNUMLABELS 900
#define MAXNUMLABELS 350
//#define MAXNUMLABELS 10

//#define MAXNUMLABELS 10


//#define CUSTSETSIZE1 10 //for small vcap, otherwise 15
//#define CUSTSETSIZE2 20
//#define NUMCOLGEN 50
//#define NUMSUBSETPRIC 20 //for small vcap, otherwise 25

//second design for larger problems
/*
#define CUSTSETSIZE1 15
#define CUSTSETSIZE2 20
#define CUSTSETSIZE3 30
#define NUMCOLGEN 70
#define NUMSUBSETPRIC1 15
#define NUMSUBSETPRIC2 30
#define NUMSUBSETPRIC 50
//for min 134
#define CUSTSETSIZE1 15
#define CUSTSETSIZE2 20
#define CUSTSETSIZE3 30
#define CUSTSETSIZE4 40
#define NUMCOLGEN 90
#define NUMSUBSETPRIC1 15
#define NUMSUBSETPRIC2 30
#define NUMSUBSETPRIC3 50
#define NUMSUBSETPRIC 70
*/

//for rand30
#define CUSTSETSIZE1 15
//#define CUSTSETSIZE1 12
#define CUSTSETSIZE2 20
#define CUSTSETSIZE3 30
#define CUSTSETSIZE4 40
#define NUMCOLGEN 70
#define NUMSUBSETPRIC1 20
//#define NUMSUBSETPRIC2 30
//#define NUMSUBSETPRIC3 50
//#define NUMSUBSETPRIC 15
#define NUMSUBSETPRIC 12


#define PRECISIONLIM 0.0001
#define MAXNODES 10000

#define LOCATION 'T'
#define TOTALPAIR 'V'
#define PAIR 'Y'
#define REQUIRED 'R'
#define FORBIDDEN 'F'
#define FREE 'E'
#define LESS 'S'
#define GREATER 'H' 
#define CONNECTED 'C'
#define NOT_CONNECTED 'N'
#define CONS 'X'
#define LESSCON 'A'
#define GREATERCON 'B'



typedef struct bbinfo {
  char v_type;
  char type;
  int dc;  //shows the index of variable for Y (PAIR-type) variables
  int r1;
  int r2;
  int n1; //keeps node1
  int n2; //node2
  struct bbinfo *next;
} BBINFO;

 
typedef struct ndinfo {
  BBINFO *info;
  struct ndinfo *parent;
} NDINFO;

typedef struct Branch_Y {
  int N1;
  int N2;
  char type;
  struct Branch_Y *next;
} BranchDec;

typedef struct Assign_constraint {
  int facility;
  int customer;
  char type;
  int  index;
  struct Assign_constraint *next;
} CONSTRAINTS;

typedef struct COL_INFO {
  int * sequence;
  int Name; 
  double Col_Time;
  int varindex;
  struct COL_INFO *next ;
} ColumnInfo ;

typedef struct Label{
  int Cap;
  double Time;
  double Cost;
  int *nodeseq; //[NCust]
  int count;
  int unreach;
  //int first_node;
  int first_node; //for solve_pri_foronefac=1;
  int last_node; //for solve_pri_foronefac=1;
} LABEL;


typedef struct Label2{
  LABEL labeldata;
  struct Label2 *next;
  struct Label2 *prev;
  char cond;
} LABEL_NODE;

// global variables to store instance
extern int NFac;      // number of facilities
extern int NCust;     // number of customers
extern double *FCost;   // facility fixed costs
extern double VFcost;   //vehicle fix cost
extern double VOcost;  //vehicle operating cost
extern int VCap; //Vehicle capacity
extern int FCap; //Facility capacity
extern double **dist;
extern int *PairNum;
extern int *Demand;
extern int source;
extern int sink;
extern int counter;
extern int TIMELIMIT;
extern char CurrMpsF[BUFSIZE];
extern int newnode;

extern char *xstat;
extern char *xstat_l;
extern char *xstat_g;
extern int *x_rhs_l;
extern int *x_rhs_g;

extern NDINFO *nodeinfo[MAXNODES];
//int num_created;
extern int numsubsetpri;
extern int stepcount;
extern int numRows;
extern ColumnInfo **Col_Array;  

extern BranchDec *Rules;
extern double heur_upper_bnd;
extern int setupperbnd;
extern int CandFAC;
extern int CandCUST;
extern int CandN1;
extern int CandN2;
extern int ApplyRule4;
extern CONSTRAINTS *ConList; //constraint list
extern int *DoCWHeurColumnGen;
extern int *max_num_labels; 
extern int *SKIPFAC;
extern int var_index;

extern int MYUB;

extern int solve_pri_foronefac;
extern int chosenFAC_ind;

extern clock_t t1,t2; 


extern int getPairNum (char *s, int *facility, int *pairNO);
extern int search_for_nodes (ColumnInfo *Pair_Arr,int pairNO,int n1,int n2,int oper);
extern int heur_column_generation_main(double **d, double **rc,int fac,double radius, double v_dual, LABEL_NODE **ColList)  ;
extern double Initial_Column_Generator(double **d,double *Xcoor, double *Ycoor, LABEL_NODE **ReturnInitCols,int *numcust, int *numcols, double closeness) ;
extern int ESPRC(double **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold,int RyanFoster,int pri_for_1_fac,int Choose_Cust_Set,int *CustomerSet);
extern int Generate_Columns(double **d, double **rc, int *Demand,int source, int sink, LABEL_NODE **ColumnList, double v_dual, int maxnumlabels,int fac,int RyanFoster, int pri_for_1_fac,int Choose_Cust_Set,int *CustomerSet,int SetSize);
extern int get_variable_index (ColumnInfo *column, int n1,int n2,int oper);
extern LABEL_NODE *Label_SORT(LABEL_NODE *list);

#endif
