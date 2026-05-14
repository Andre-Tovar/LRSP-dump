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
 * A_MPS.C
 */
//new constraint total facility>= total necessary facility
//Explicit branching
extern "C"{  
#include "header.h"
#include "minto.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
}
/*
 * appl_mps
 */ 
/*
int id;               /* identification of active minto 
int *vcnt;            /* number of variables 
int *ccnt;            /* number of constraints 
int *nzcnt;           /* number of nonzero's 
double **vobj;        /* objective coefficients of the variables 
double **vlb;         /* lower bounds on the variables 
double **vub;         /* upper bounds on the variables 
char **vclass;        /* types of the variables, i.e., 'C', 'B', or 'I' 
char **csense;        /* senses of the constraints, i.e., 'L', 'E', or 'G' 
double **crhs;        /* right hand sides of the constraints 
int **vfirst;         /* starting positions of the columns of the variables 
int **vind;           /* indices of the nonzero coefficients 
double **vcoef;       /* nonzero coefficients 
int *vstorsz;         /* total number of characters in the names of the variables 
char **vstore;        /* names of the variables 
char ***vname;        /* starting positions of the names of the variables 
int *cstorsz;         /* total number of characters in the names of the constraints  
char **cstore;        /* names of the constraints 
char ***cname;        /* starting positions of the names of the constraints 
*/
unsigned int
appl_mps(int id,
	          int *vcnt,
	          int *ccnt,
	          int *nzcnt,
	          double **vobj,
	          double **vlb,
	          double **vub,
	          char **vclass,
	          char **csense,
	          double **crhs,
	          int **vfirst,
	          int **vind,
	          double **vcoef,
	          int *vstorsz,
	          char **vstore,
	          char ***vname,
	          int *cstorsz,
	          char **cstore,
	          char ***cname)
{

  char buf[BUFSIZE];
  FILE *fp;
  int Nodes,i,j,k;
  double f;
  //double **CostOfPair;
  //double *distance;
  int numCols,numNZ, infeasible;
  //int *CurCap; //cuurent capacity of a facility.
  //int *paircount;
  //int *InSoln;
  double *Xcoor, *Ycoor;
  int *Cno;
  double *_vobj;
  double *_vlb;
  double *_vub;
  char *_vtype;
  char *_csense;
  double *_crhs;
  int *_vfirst;
  int *_vind;
  double *_vcoef;
  char *_vstore;
  char **_vname;
  char *_cstore;
  char **_cname;
  LABEL_NODE *List,*temp1,*temp2;
  // int **d;
  int numcust,num_cols;
  // double **distcopy;
  int TDemand;
  double num_REQ1;
  ColumnInfo *column;
  double FCostA, max_fixed,OCArtificial;
  double *STime;
   double frac;
   int num_REQ;
 

  // if recursive version of MINTO, read formulation from mps file
  if (id) {
     fprintf(stdout, "In appl_mps, about to return YES\n");
     return (YES);
  } // if id

 Cno=0;
 Xcoor=0;
 Ycoor=0;
 STime=0;

 t1=clock();

   /* READ PROBLEM INSTANCE */

  sprintf(buf, "%s", inq_file());
  if ( (fp = fopen(buf, "r")) == NULL) {
    fprintf(stdout, "Unable to open %s\n", buf);
    exit(9);
  } // if
       
  fscanf(fp, "%d %d", &NFac, &NCust);
  //fprintf(stdout, "Number of facilities: %d\n", NFac);
  //fprintf(stdout, "Number of customers: %d\n", NCust);
  Nodes = NFac+ NCust;
  
  FCost = (double *) calloc(NFac, sizeof(double));
  
  for (i=0;i < NFac;i++) {
    fscanf(fp, "%lf ", &f);
    FCost[i]=f ;
    //  printf( "Fix Cost for facility %d %lf  \n", i, FCost[i]);
  } 
  
  fscanf(fp, "%lf %lf", &VFcost, &VOcost); 
  //printf("Fix Cost and variable cost for vehicle: %lf  %lf \n", VFcost,VOcost);
  
  fscanf(fp, "%d %d %d", &VCap, &FCap,&TIMELIMIT); 
  // fprintf(stderr, "Vehicle capacity %d and facility capacity %d \n", VCap,FCap);
 
  Xcoor = (double*) calloc(Nodes, sizeof(double));
  Ycoor = (double *) calloc(Nodes, sizeof(double));
  Cno = (int *) calloc(Nodes, sizeof(int));
  STime = (double *) calloc(Nodes, sizeof(double));
  Demand = (int *) calloc(Nodes, sizeof(int));
  
  //1..NCust:customer number, NCust+1..Nodes:Facilities
  
  
  //For Cordeau instances
  for (j=0;j < Nodes;j++) {
    fscanf(fp, "%d %lf %lf %lf %d \n", &Cno[j],&Xcoor[j],&Ycoor[j],&STime[j],&Demand[j]);
  }

  TDemand=0;
  for (j=0;j < NCust;j++) {
    TDemand=TDemand+Demand[j];
  }
  num_REQ1=(double) TDemand/FCap;
  num_REQ=(int)TDemand/FCap;
  if (num_REQ<num_REQ1)
    num_REQ=num_REQ+1;
  
  printf("num_REQ=%d \n",num_REQ);
  
  //***********************************************************//
  //***Determine the fixed cost of an artificial facility *****//
  max_fixed=FCost[0];
  for (j=1;j<NFac;j++){
    if (FCost[j]>max_fixed)
      max_fixed=FCost[j];
  }
  FCostA=(double)(num_REQ+2)*max_fixed+1;
  //**********************************************************//
  

  dist = (double **) calloc(Nodes, sizeof(double *));
  for (i=0; i < Nodes; i++)
    dist[i] = (double *) calloc(Nodes, sizeof(double));

  for (i=0;i<Nodes;i++) {
    for (j=0;j<Nodes;j++){
      dist[i][j]=sqrt((Xcoor[i]-Xcoor[j])*(Xcoor[i]-Xcoor[j])+(Ycoor[i]-Ycoor[j])*(Ycoor[i]-Ycoor[j]));
    }
  }
  
  for (i=0;i<Nodes;i++) {
    for (j=i;j<Nodes;j++){
      if (i!=j){
	dist[i][j]=dist[i][j]+(STime[i]+STime[j])/2;
	dist[j][i]=dist[i][j];
      }
      else
	dist[i][j]=0;
    }
  }


  for (i=0;i<Nodes;i++) {
    for (j=i;j<Nodes;j++){
      frac=dist[i][j]-floor(dist[i][j]);
      if (frac>=0.5)
	dist[i][j]=ceil(dist[i][j]);
      else
	dist[i][j]=floor(dist[i][j]);
      dist[j][i]=dist[i][j];   
    }
  }
  

  //Check for Triangular in equality:
  
  for (k=0; k < Nodes; k++) {
    for (i=0; i < Nodes; i++) {
      for (j=0; j < Nodes; j++) {
	if (dist[i][j] > dist[i][k] + dist[k][j])
	  dist[i][j] = dist[i][k] + dist[k][j];
      }
    } 
  } 
 
  

  ///***********************************************/
  //**** Calculate OCArtificial: ****/ 
  //pick first facility
  OCArtificial=0;
  for(k=0; k < NCust; k++)
    OCArtificial=(double)OCArtificial+2*dist[NCust][k];
 
  OCArtificial=(double)OCArtificial*VOcost/60+VFcost;

  //***********************************************/

  //check whether the data is infeasible or not/
  //check the reachability within time limit. do not check capacity< truck capacity
  for (i=0;i<NCust;i++){
    for (j=0;j<NFac;j++){
      infeasible=1;
      if (dist[NCust+j][i]+dist[i][NCust+j]<=TIMELIMIT){
	infeasible=0;
	break;
      }
    }
    if (infeasible){
      printf("data is infeasible, we can not reach customer %d, within time limit\n",i);
      exit(9);
      break;
    }
  }

    
  PairNum=(int*)calloc (NFac,sizeof(int));
  
  /////////////initial column generation  ////////////////////////////////
  
  numRows = NFac+NCust+NCust*NFac+1+NFac+NCust; 
  //NCust=SP consts + NFac= Fac Cap. Const, NCust*NFac=Relation T, Y, 1=lower bnd for total T, 
  // NFac for explicit branching, +NCust constraints for the artificial variables
  
  numCols = NFac+NFac+1+NCust; //add NFac for new variables v_j+1 for artificial facility+Ncust for artificial pairing columns
  
  numNZ = NFac+NCust*NFac+NFac+NFac+NCust+NCust+NCust; //add NFac add NFac for each variable v_j
  //NFac:Fac. Cap const, NCust*NFac:Relation, NFac=Total fac., NFac:V variables, 2*NCust: artificial const,  NCust: artificial pairings in SP
  

  
  
  /******************  GENERATE INITIAL COLUMNS AND FIND AN UPPER BOUND   *****/
  List=0;
  heur_upper_bnd = Initial_Column_Generator(dist,Xcoor,Ycoor, &List,&numcust, &num_cols, 0.8) ;
  //0.8:when assigning look at radius 0.4 of facilities.  
  //heur_upper_bnd = Initial_Column_Generator(distcopy,Xcoor,Ycoor, &List,&numcust, &num_cols, 0.8) ;
 
  /****************** ********************************************************/
  
  numCols=numCols+num_cols;
  numNZ =numNZ+numcust+num_cols+numcust+num_cols; //add success for total pairings 
  
  /*
  count_list=0;
  temp1=List;
  while(temp1) {
    count_list++;
    printf("fac=%d, route= ",temp1->labeldata.unreach);
    for (j=0; j < NCust; j++) {
      if (temp1->labeldata.nodeseq[j]> 0) { 
       printf("%d:%d ",j,temp1->labeldata.nodeseq[j]);
     } // if 
   } // for j
    printf("\n");
    temp1=temp1->next;
  }
    
  printf("# of columns in the list=%d \n",count_list);
  */

  //**************** Keeps the columns generated for each facility *************/
  Col_Array = (ColumnInfo **)calloc (NFac,sizeof (ColumnInfo*));
  //***************************************************************************/
  
  //  printf("NumNZ=%d ,NumCol=%d ,NumRow=%d \n",numNZ,numCols,numRows);
  
  
  _vobj = (double *) calloc(numCols, sizeof(double));
  _vlb = (double *) calloc(numCols, sizeof(double));
  _vub = (double *) calloc(numCols, sizeof(double));
  _vtype = (char *) calloc(numCols, sizeof(char));
  _csense = (char *) calloc(numRows, sizeof(char));
  _crhs = (double *) calloc(numRows, sizeof(double));
  _vfirst = (int *) calloc(numCols+1, sizeof(int));
  _vind = (int *) calloc(numNZ, sizeof(int));
  _vcoef = (double *) calloc(numNZ, sizeof(double));
  
  *vstorsz = NAME_SIZE*numCols;
  *cstorsz = NAME_SIZE*numRows;
  
  _vstore = (char *) calloc(*vstorsz, sizeof(char));
  _cstore = (char *) calloc(*cstorsz, sizeof(char));
  _vname = (char **) calloc(numCols, sizeof(char *));
  _cname = (char **) calloc(numRows, sizeof(char *));
  
  
  //Constraints:
  
  // set partitioning constraints 
  *ccnt = 0;
  for (i=0; i < NCust; i++) {
    _crhs[*ccnt] = (double) 1;
    _csense[*ccnt] = 'E';
    sprintf(&(_cstore[NAME_SIZE*(*ccnt)]), "C_%d", i);
    _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
    _cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
    (*ccnt)++;
  } 
  
  // facility  capacity constraints
  
  for (j=0; j < NFac; j++) {
    //for knapsack cuts change it:
    //  _crhs[*ccnt] = (double) -FCap;
    _crhs[*ccnt] = (double) 0;
    _csense[*ccnt] = 'G';
    sprintf(&(_cstore[NAME_SIZE*(*ccnt)]), "C_%d", NCust+j);
    _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
    _cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
    (*ccnt)++;
  } 
  
  //3rd set of constraint: relation between Tj and Zjp variables
  
  for (j=0; j < NFac; j++) {
    for (i=0; i < NCust; i++) {
      _crhs[*ccnt] = (double) 0;
      _csense[*ccnt] = 'G';
      sprintf(&(_cstore[NAME_SIZE*(*ccnt)]), "C_%d_%d", j, i);
      _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
      _cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
      (*ccnt)++;
    } 
  } 
  //new ///////////////////////
  //total open facility constraint:
  _crhs[*ccnt] = (double)num_REQ ;
  _csense[*ccnt] = 'G';
  sprintf(&(_cstore[NAME_SIZE*(*ccnt)]), "F");
  _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
  _cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
  (*ccnt)++;
  ////////////////////////////////
  
  //new constraint :total pairing=v_j 
  for (j=0; j < NFac; j++) {
    _crhs[*ccnt] = (double) 0;
    _csense[*ccnt] = 'E';
    sprintf(&(_cstore[NAME_SIZE*(*ccnt)]), "E_%d",j);
    _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
    _cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
    (*ccnt)++;
  } 
  
  //Zmj<=Tm constraint
 for (i=0; i < NCust; i++) {
   _crhs[*ccnt] = (double) 0;
   _csense[*ccnt] = 'G';
   sprintf(&(_cstore[NAME_SIZE*(*ccnt)]), "Art_%d",i);
   _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
   _cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
   (*ccnt)++;
 } 
 
 
 var_index=-1; //will calculate the index of each variable in the model
  
 /********************************* VARIABLES **********************************/ 
 
 vfirst[0] = 0;
 *vcnt = 0;
 *nzcnt = 0;
 
 // Tj:location variables
 
 for (j=0; j < NFac; j++) {
   
   sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "T_%d", j);
   _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
   _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);
  
  
   _vind[*nzcnt] = NCust + j;
   _vcoef[*nzcnt] = (double) FCap;
   (*nzcnt)++;

   for (i=0; i < NCust; i++) {
     _vind[*nzcnt] = NCust + NFac+ j*NCust + i;
     _vcoef[(*nzcnt)++] =(double)  1;
   } 
   
  
   _vind[*nzcnt] = NCust + NFac+ NFac*NCust ;
   _vcoef[(*nzcnt)++] =(double)  1;
 
   
   _vobj[*vcnt] = (double) FCost[j];
   _vlb[*vcnt] =  (double) 0;
   _vub[*vcnt] =  (double) 1;
   _vtype[*vcnt] = 'B'; 
   _vfirst[++(*vcnt)] = *nzcnt;
   var_index++;

 } 
 
 
 //new variable v_j 
 for (j=0; j < NFac; j++) {
   
   sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "V_%d", j);
   _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
   _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);

   _vind[*nzcnt] = NCust + NFac+NCust*NFac+1+j;
   _vcoef[*nzcnt] = - (double) 1;
   (*nzcnt)++;

   _vobj[*vcnt] = (double) 0;
   _vlb[*vcnt] =  (double) 0;
   _vub[*vcnt] =  (double) NCust;
   // _vub[*vcnt] =  (double) 12;
   _vtype[*vcnt] = 'I'; 
   _vfirst[++(*vcnt)] = *nzcnt;

    var_index++;
 } 

 // Tm:artificial location variable
 
 sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "T_M");
 _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
 _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);


 for (j=0; j < NCust; j++) {
   _vind[*nzcnt] = NFac+NCust+NCust*NFac+1+NFac+j;
   _vcoef[(*nzcnt)++] = (double) 1;
 }
 
 _vobj[*vcnt] = (double) FCostA;
 
 _vlb[*vcnt] =  (double) 0;
 _vub[*vcnt] =  (double) 1;
 _vtype[*vcnt] = 'B'; 
 _vfirst[++(*vcnt)] = *nzcnt;
 
 var_index++;

 //Artificial Pairing variable, 1 column for each customer  
 
 for (j=0; j < NCust; j++) {
   sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "A_M_%d", j);
   _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
   _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);
   
   //set partitioning constr
   _vind[*nzcnt] =j;
   _vcoef[(*nzcnt)++] = (double) 1;
   
   //relation btw fac. and variable
   _vind[*nzcnt] = NFac+NCust+NCust*NFac+1+NFac+j;
   _vcoef[(*nzcnt)++] = -(double) 1;
   
   _vobj[*vcnt] = (double) OCArtificial;
   _vlb[*vcnt] =  (double)0;
   _vub[*vcnt] = (double)1;
   _vtype[*vcnt] = 'B';
   _vfirst[++(*vcnt)] = *nzcnt;
   var_index++;
 }
 
 
 // initial set of pair columns one for each customer: Yjp
 

 for (i=0;i<NFac;i++){
   Col_Array[i]=0;
   PairNum[i]=0;
 }

 temp1=List;
 while (temp1) { 
   i=temp1->labeldata.unreach;
   var_index++;
   //********************************************
   column= (ColumnInfo *) calloc (1, sizeof(ColumnInfo));
   column->sequence=(int *) calloc (NCust, sizeof(int));
     //**********************************************
   
   sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "Y_%d_%d", i, PairNum[i]);
   _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
   _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);

   //***************************     
   column->Name=PairNum[i];
   column->Col_Time=temp1->labeldata.Time;
   column->varindex=var_index;
  
  

   for (j=0; j < NCust; j++) {
     if (temp1->labeldata.nodeseq[j]> 0) { 
       _vind[*nzcnt] = j;
       _vcoef[(*nzcnt)++] = (double)1;
       column->sequence[j]=temp1->labeldata.nodeseq[j];
     } // if 
   } // for j
   
  
   _vind[*nzcnt]=NCust+i;
   _vcoef[(*nzcnt)++]=  -(double) temp1->labeldata.Cap;
     
   for (j=0;j<NCust;j++) {
     if (temp1->labeldata.nodeseq[j]>0) {
       _vind[*nzcnt] = NCust+NFac+i*NCust+j;
       _vcoef[(*nzcnt)++] = (double) -1;
     }
   }
 
   _vind[*nzcnt]=NCust+NFac+NCust*NFac+1+i;
   _vcoef[(*nzcnt)++]=  (double) 1;

   
   _vobj[*vcnt] = (double)(temp1->labeldata.Time)*VOcost/60+VFcost;
   _vlb[*vcnt] =  (double)0;
   _vub[*vcnt] = (double)1;
   _vtype[*vcnt] = 'B';
   _vfirst[++(*vcnt)] = *nzcnt;
   
   temp2=temp1;
   temp1=temp1->next;
   
   if (temp1)
     temp1->prev=0;

   temp2->next=0;
   free(temp2->labeldata.nodeseq);
   free(temp2);
   PairNum[i]++;
   
   //****************************
   column->next=Col_Array[i];
   Col_Array[i]=column;
   column=0;
   //**************************
   
 }//while
   
 
 *vobj = _vobj;
 *vlb = _vlb;
 *vub = _vub;
 *vclass = _vtype;
 *csense = _csense;
 *crhs = _crhs;
 *vfirst = _vfirst;
 *vind = _vind;
 *vcoef = _vcoef;
 *vstore = _vstore;
 *vname = _vname;
 *cstore = _cstore;
 *cname = _cname;
 



 /*
 for(i=0;i<NFac;i++){
   printf("pairNum[%d]:%d\n",i,PairNum[i]);
 }
 */
 
 /*
   column=Col_Array[0];
   while(column){
   for (i=0;i<NCust;i++){
   if (column->sequence[i])
   printf("%d: %d ",i,column->sequence[i]);
   }
   printf("\n");
   column=column->next;
   }
   exit(1);
 */
 
 /************** parameter initialization ******************/
 stepcount=0;
 counter=1; 
 
 //heuristic UPPER BOUND    
 setupperbnd=0;
 
 max_num_labels=(int*)calloc(NFac,sizeof(int));
 for (i=0;i<NFac;i++)
   max_num_labels[i]=5;
 
  
 DoCWHeurColumnGen=(int*)calloc(NFac,sizeof(int));
 for (i=0;i<NFac;i++)
   DoCWHeurColumnGen[i]=0;
 
 SKIPFAC=(int*)calloc(NFac,sizeof(int));
 for (i=0;i<NFac;i++)
   SKIPFAC[i]=0;
 
 if (Cno)
   free (Cno);
 if (Xcoor)
   free(Xcoor);
 if (Ycoor)
   free (Ycoor);
 
 Cno=0;
 Xcoor=0;
 Ycoor=0;

 if (STime)
   free (STime);
 STime=0;

 if (List)
   List=0;
 
 MYUB=1;
 //num_debug = 0;

 return (NO);

} // appl_mps()
