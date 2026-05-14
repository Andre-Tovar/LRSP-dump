//change structure for the domination rule..
//add prev, nodeseq..

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "minto.h"

#define BUFSIZE 64
#define NAME_SIZE 16
#define COST_PER_MIN 1.0
#define MY_INF 1000000000
#define NUMNODES 500
//#define TIMELIMIT 150
#define MAXNUMLABELS 700
#define PRECISIONLIM 0.0001

#define MAXNODES 10000

#define LOCATION 'T'
#define TOTALPAIR 'V'
#define PAIR 'Y'
//#define PATH 'Y'
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


typedef struct MY_VAR {
  int * my_path;
  char *my_name;
  int var_no;
  int size;
  struct MY_VAR *next; 
} my_var;

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
  struct Assign_constraint *next;
} CONSTRAINTS;


typedef struct PairVar { //keep the branching information
  int index;
  struct PairVar *next;
} Pair_VAR;

/*typedef struct y_branch {
  int indexofvar;
  int valueofvar;
  int numberofrules;
  char uporlow;
  struct y_branch *next;
} Y_BRANCH;
*/
typedef struct COL_INFO {
  int * sequence;
  int Name; 
  int Col_Time;
  struct COL_INFO *next ;
} ColumnInfo ;

typedef struct CandidateList {
  int n1;
  int n2;
  double frac;
  struct CandidateList *next;
} CandLIST;

typedef struct Label{
  int Cap;
  int Time;
  double Cost;
  int *nodeseq; //[NCust]
  int count;
  int unreach;
} LABEL;


typedef struct Label2{
  LABEL labeldata;
  struct Label2 *next;
  struct Label2 *prev;
  char cond;
} LABEL_NODE;




// global variables to store instance
int NFac;      // number of facilities
int NCust;     // number of customers
double *FCost;   // facility fixed costs
double VFcost;   //vehicle fix cost
double VOcost;  //vehicle operating cost
//int VFcost;   //vehicle fix cost
//int VOcost;  //vehicle operating cost

int VCap; //Vehicle capacity
int FCap; //Facility capacity
int TCap; //time capacity
double **dist; //distance matrix
double *STime;
int *PairNum;
int *Demand;
int source;
int sink;
int NumColGen;
int counter;
int zel;
int TotalCol;
int last_soln;
my_var *VarArr;
int TIMELIMIT;
char CurrMpsF[BUFSIZE];
int NoTvar;
int newnode;
int newcons;
char *xstat;
char *xstat_l;
char *xstat_g;
int *x_rhs_l;
int *x_rhs_g;
//Y_BRANCH *Y_Rules;
NDINFO *nodeinfo[MAXNODES];
int num_created;
int num_REQ;
int stepcount;
int numRows;
ColumnInfo **Col_Array;  
//int col_generation;
int Node1;
int Node2;
BranchDec *Rules;
Pair_VAR *PairBounds;
int fathomnode;  //if there is artificial variables in the lp solution or not
CandLIST *NodeList;
double heur_upper_bnd;
int setupperbnd;
int CandFAC;
int CandCUST;
int ApplyRule4;
CONSTRAINTS *ConList; //constraint list
int nnewcon;
// FILE *DUALS;
//  char Dual_info[BUFSIZE];
int *SetThreshold;
int *DoCWHeurColumnGen;
int *Threshold_a;
int *max_num_labels; 
int *SKIPFAC;


int getPairNum (char *s, int *facility, int *pairNO);
int search_for_nodes (ColumnInfo *Pair_Arr,int pairNO,int n1,int n2,int oper);
double heurmain2(double **d) ;
int heur_column_generation_main(int **d, double **rc,int fac,double radius, double v_dual, LABEL_NODE **ColList)  ;
int ESPRC (int **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold);
int Generate_Columns(int **d, double **rc, int *Demand,int source, int sink, LABEL_NODE **ColumnList, double v_dual, int maxnumlabels,int fac) ;
