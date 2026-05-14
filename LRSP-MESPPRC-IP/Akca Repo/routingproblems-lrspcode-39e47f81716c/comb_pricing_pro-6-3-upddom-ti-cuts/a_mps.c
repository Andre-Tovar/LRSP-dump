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

//initial columns are generated. columns with 1,2 and 3 customers are generated.. 
//new constraint total facility>= total necessary facility
//Explicit branching
 
#include "header.h"



/*
 * appl_mps
 */ 
unsigned
appl_mps (id, vcnt, ccnt, nzcnt, vobj, vlb, vub, vclass, csense, crhs,
          vfirst, vind, vcoef, vstorsz, vstore, vname, cstorsz, cstore, cname)
int id;               /* identification of active minto */
int *vcnt;            /* number of variables */
int *ccnt;            /* number of constraints */
int *nzcnt;           /* number of nonzero's */
double **vobj;        /* objective coefficients of the variables */
double **vlb;         /* lower bounds on the variables */
double **vub;         /* upper bounds on the variables */
char **vclass;        /* types of the variables, i.e., 'C', 'B', or 'I' */
char **csense;        /* senses of the constraints, i.e., 'L', 'E', or 'G' */
double **crhs;        /* right hand sides of the constraints */
int **vfirst;         /* starting positions of the columns of the variables */
int **vind;           /* indices of the nonzero coefficients */
double **vcoef;       /* nonzero coefficients */
int *vstorsz;         /* total number of characters in the names of the variables */ 
char **vstore;        /* names of the variables */
char ***vname;        /* starting positions of the names of the variables */
int *cstorsz;         /* total number of characters in the names of the constraints */ 
char **cstore;        /* names of the constraints */
char ***cname;        /* starting positions of the names of the constraints */
{
  char buf[BUFSIZE],initnum_file[BUFSIZE],initcol_file[BUFSIZE], UpperBnd_file[BUFSIZE];
  FILE *fp, *numF, *colF, *UpperBnd;
  int Nodes;
  int i,j,k,t,t1;
  double f;
  //double **CostOfPair;
  //double *distance;
  int numCols,numNZ;
  //int *CurCap; //cuurent capacity of a facility.
  //int *paircount;
  int infeasible;
  //int *InSoln;
  double *Xcoor;
  double *Ycoor;
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
  // my_var *temp;
  int TnumCol;

  int **d;
  int success,numcust;
  double **distcopy;
  int TDemand;
  double num_REQ1;
  ColumnInfo *column;
  double FCostA;
  double max_fixed;
  double OCArtificial;
  int fac,cap,count,numselected, *HeurFac, *HeurPairNum;
 
  // if recursive version of MINTO, read formulation from mps file
  if (id) {
     fprintf(stdout, "In appl_mps, about to return YES\n");
     return (YES);
  } // if id

  fprintf(stdout, " a_mps.c is running\n");
  last_soln=0;
  /* READ PROBLEM INSTANCE */

  sprintf(buf, "%s", inq_file());
  if ( (fp = fopen(buf, "r")) == NULL) {
    fprintf(stdout, "Unable to open %s\n", buf);
    exit(9);
  } // if
       
  fscanf(fp, "%d %d", &NFac, &NCust);
  fprintf(stdout, "Number of facilities: %d\n", NFac);
  fprintf(stdout, "Number of customers: %d\n", NCust);
  Nodes = NFac+ NCust;
 
  FCost = (double *) calloc(NFac, sizeof(double));

  for (i=0;i < NFac;i++) {
    fscanf(fp, "%lf ", &f);
    FCost[i]=f ;
    //fprintf(stderr, "Fix Cost for facility %d %d   \n", i, FCost[i]);
  } 
  
  fscanf(fp, "%lf %lf", &VFcost, &VOcost); 
  //fscanf(fp, "%d %d", &VFcost, &VOcost);
  // fprintf(stderr, "Fix Cost and variable cost for vehicle: %lf  %lf \n", VFcost,VOcost);
  
  fscanf(fp, "%d %d %d", &VCap, &FCap,&TIMELIMIT); 
  // fprintf(stderr, "Vehicle capacity %d and facility capacity %d \n", VCap,FCap);
 
 Xcoor = (double*) calloc(Nodes, sizeof(double));
 Ycoor = (double *) calloc(Nodes, sizeof(double));
 Cno = (int *) calloc(Nodes, sizeof(int));
 STime = (double *) calloc(Nodes, sizeof(double));
 Demand = (int *) calloc(Nodes, sizeof(int));

 //1..NCust:customer number, NCust+1..Nodes:Facilities
 
 
//working for cor25
 for (j=0;j < Nodes;j++) {
   fscanf(fp, "%d %lf %lf %lf %d \n", &Cno[j],&Xcoor[j],&Ycoor[j],&STime[j],&Demand[j]);
 }
 TDemand=0;
 for (j=0;j < NCust;j++) {
   TDemand=TDemand+Demand[j];
 }
 num_REQ1=(double) TDemand/FCap;
 num_REQ=TDemand/FCap;
 if (num_REQ<num_REQ1)
   num_REQ=num_REQ+1;

 printf("num_REQ=%d \n",num_REQ);

 //***********************************************************8//
 //***Determine the fixed cost of an artificial facility *****//
 max_fixed=FCost[0];
 for (j=1;j<NFac;j++){
   if (FCost[j]>max_fixed)
     max_fixed=FCost[j];
 }
 FCostA=num_REQ*max_fixed+1;
 //**********************************************************//


 
/*
 //work for sample.vrp
 for (j=0;j < Nodes;j++) {
   fscanf(fp, "%d %lf %lf %lf\n", &Cno[j],&Xcoor[j],&Ycoor[j],&STime[j]);
 }
 for (j=0;j < Nodes;j++) {
   fscanf(fp, "%d %d\n", &Cno[j],&Demand[j]);
 }
 */
 dist = (double **) calloc(Nodes, sizeof(double *));
 for (i=0; i < Nodes; i++)
   dist[i] = (double *) calloc(Nodes, sizeof(double));
 
 d = (int **) calloc(Nodes, sizeof(int *));
 for (i=0; i < Nodes; i++)
   d[i] = (int *) calloc(Nodes, sizeof(int));
 
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
 /*
   fprintf(stderr,"Distance matrix:\n");
   for (i=0;i<Nodes;i++) {
     for (j=0;j<Nodes;j++){
       fprintf(stderr, "%5.1lf  ", dist[i][j]);
     }
     fprintf(stderr,"\n");
   }
 */

  for (i=0;i<Nodes;i++) {
    for (j=i;j<Nodes;j++){
      d[i][j]=dist[i][j];
      d[j][i]=d[i][j];
      dist[i][j]=d[i][j];
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
 
  

  ///***********************************************8
  //**** Calculate OCArtificial: 
  //pick first facility
  OCArtificial=0;
  for(k=0; k < NCust; k++)
    OCArtificial=OCArtificial+2*dist[NCust][k];

  OCArtificial=OCArtificial*VOcost/60+VFcost;

  //**************************************

 


  //check whether the data is infeasible or not/
  //check the reachability within time limit. do not check capacity< truck capacity
  for (i=0;i<NCust;i++){
    for (j=0;j<NFac;j++){
      infeasible=1;
      if (2*dist[NCust+j][i]<=TIMELIMIT){
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
  /*
 
   fprintf(stderr,"Distance matrix:\n");
   for (i=0;i<Nodes;i++) {
     for (j=0;j<Nodes;j++){
       fprintf(stderr, "%5.1lf  ", dist[i][j]);
     }
     fprintf(stderr,"\n");
   }
  */
   
 PairNum=(int*)calloc (NFac,sizeof(int));
	
    /////////////initial column generation  ////////////////////////////////
 numRows = NFac+NCust+NCust*NFac+1+NFac+NCust; //add 1, add NFac for explicit branching, +NCust constraints for the artificial variables
 
 numCols = NFac+NFac+1+NCust; //add NFac for new variables v_j+1 for artificial facility+Ncust for artificial pairing columns
 
 /* For knapsack cuts */
 //numNZ = NCust*NFac+NFac+NFac+NCust+NCust+NCust;
 
//for non knapsack cuts
 numNZ = NFac+NCust*NFac+NFac+NFac+NCust+NCust+NCust; //add NFac add NFac for each variable v_j
 //1 artificial column for each customer in set partitioning constraint thus add NCust
 //for Zmj<=Tm: NCust+NCust
 

 sprintf(initnum_file, "%s.num", inq_file());
 if ( (numF = fopen(initnum_file, "r")) == NULL) {
   fprintf(stdout, "Unable to open %s\n", initnum_file);
   exit(9);
 } // if
 
 fscanf(numF, "%d %d", &success, &numcust);
 printf("# of columns=%d, # of customers=%d \n",success,numcust);
 
 for (i=0;i<NFac;i++) {
   fscanf(numF, "%d \n", &PairNum[i]);
   printf("PairNum[%d]=%d \n",i,PairNum[i]);
 } 
 
 
 numCols=numCols+success;
 numNZ =numNZ+numcust+success+numcust+success; //add success for total pairings 
 


 //********************************************************
 Col_Array = (ColumnInfo **)calloc (NFac,sizeof (ColumnInfo*));
 //**********************************************************

 printf("NumNZ=%d ,NumCol=%d ,NumRow=%d \n",numNZ,numCols,numRows);
 
 
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

 var_index=-1;

 /* VARIABLES */ 
 
 vfirst[0] = 0;
 *vcnt = 0;
 *nzcnt = 0;
 
 // Tj:location variables
  
 for (j=0; j < NFac; j++) {
   
   sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "T_%d", j);
   _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
   _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);
  
   //for knapsack cuts close this
   _vind[*nzcnt] = NCust + j;
   _vcoef[*nzcnt] = (double) FCap;
   (*nzcnt)++;

   for (i=0; i < NCust; i++) {
     _vind[*nzcnt] = NCust + NFac+ j*NCust + i;
     _vcoef[(*nzcnt)++] =(double)  1;
   } 
   
   //add///////////////////
   _vind[*nzcnt] = NCust + NFac+ NFac*NCust ;
   _vcoef[(*nzcnt)++] =(double)  1;
   //add //////////////////
   
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

 sprintf(initcol_file, "%s.init", inq_file());
 if ( (colF = fopen(initcol_file, "r")) == NULL) {
   fprintf(stdout, "Unable to open %s\n", initcol_file);
   exit(9);
 } // if
       

 for (i=0;i<NFac;i++){
   k=0;
   Col_Array[i]=0; 
   fac=i;
   while (k<PairNum[i]){ 
       var_index++;
     // printf("PairNum[%d]=%d \n",i,PairNum[i]);
     //********************************************
     column= (ColumnInfo *) calloc (1, sizeof(ColumnInfo));
     column->sequence=(int *) calloc (NCust, sizeof(int));
     //**********************************************
  
     sprintf(&(_vstore[NAME_SIZE*(*vcnt)]), "Y_%d_%d", i, k);
     _vstore[NAME_SIZE*(*vcnt + 1) - 1] = '\0';
     _vname[*vcnt] = &(_vstore[NAME_SIZE*(*vcnt)]);
     
     //***************************     
     column->Name=k;
     column->varindex=var_index;
     //*****************************************
     fscanf(colF, "%d %d %d %d", &fac, &column->Col_Time,&cap,&t);
     // printf("%d    %d      %d      %d  ",fac, column->Col_Time,cap,t);
     if(fac!=i) {
       printf("The data file can not be read correctly, fac=%d, i=%d, k=%d \n",fac,i,k);
       exit(9);
     }
     while (t!=-1){  
       fscanf(colF, " %d ",&t1);  
       column->sequence[t]=t1;
       fscanf(colF, " %d ",&t); 
     }
     
     fscanf(colF, "\n"); 
     //printf(" \n"); 
     
     //***************************************
     for(t=0;t<NCust;t++){
       if (column->sequence[t]){
	 _vind[*nzcnt] =t;
	 _vcoef[(*nzcnt)++] = (double) 1;
       }
     }
    
     _vind[*nzcnt]=NCust+i;
     _vcoef[(*nzcnt)++]=  -(double) cap;
     
     for(t=0;t<NCust;t++){
       if (column->sequence[t]){
	 _vind[*nzcnt]=NCust+NFac+i*NCust+t;
	 _vcoef[(*nzcnt)++]= -(double) 1;
       }
     }
     
     _vind[*nzcnt]=NCust+NFac+NCust*NFac+1+i;
     _vcoef[(*nzcnt)++]=  (double) 1;


     _vobj[*vcnt] = (double)(column->Col_Time)*VOcost/60+VFcost;
     _vlb[*vcnt] =  (double)0;
     _vub[*vcnt] = (double)1;
     _vtype[*vcnt] = 'B';
     _vfirst[++(*vcnt)] = *nzcnt;
     
     
     k++;
     
     //****************************
     column->next=Col_Array[i];
     Col_Array[i]=column;
     //**************************

   }//while
   printf("When exit loop of facility %d, k =%d \n",i,k);

 }//for all i, Facilities
 
 

 
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

  //  stepcount=0;
  counter=1; 
  // col_generation=1;

  fprintf(stdout,"end of a_mps.c file\n");
  for(i=0;i<NFac;i++){
    printf("pairNum[%d]:%d\n",i,PairNum[i]);
  }
  stepcount=0;
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
  
  //sprintf(Dual_info,"%s.dual",inq_file());
  //DUALS=fopen(Dual_info,"w");

  
  //Use an UPPER BOUND that is found by solving the tree heuristically
  sprintf(UpperBnd_file, "%s.ub", inq_file());
  if ( (UpperBnd = fopen(UpperBnd_file, "r")) == NULL) {
    fprintf(stdout, "Unable to open %s\n",UpperBnd_file);
    exit(9);
  } // if
 
  fscanf(UpperBnd, "%lf\n", &heur_upper_bnd);
  heur_upper_bnd=-heur_upper_bnd;
  
  /*
  //Calculate a heuristic UPPER BOUND   using C-W Heurisitic 
  heur_upper_bnd=heurmain2(dist);
  */

  setupperbnd=0;
  




  SetThreshold=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
    SetThreshold[i]=1;

  max_num_labels=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
     max_num_labels[i]=400;


 
  DoCWHeurColumnGen=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
    DoCWHeurColumnGen[i]=1;

  SKIPFAC=(int*)calloc(NFac,sizeof(int));
  for (i=0;i<NFac;i++)
    SKIPFAC[i]=0;

  

  num_gap_cuts=0;
  Generate_GAP_Cuts=1;
  num_rule4_cuts=0;

  GapConList=0;
  ConList=0;

  free (Cno);
  free(Xcoor);
  free (Ycoor);
  for (i=0;i<Nodes;i++)
    free(d[i]);
  free(d);
  // exit(1);
  return (NO);
  // return (YES);
} // appl_mps()
