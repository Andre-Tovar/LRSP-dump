//Sep.10 2006 C-W columns +1 cust columns + 
//nearest neighbor, sweep, nearest insertion heuristic is added.



//May 30, 2006 Heuristic columns + columns with 1 customer only

//13 March 2006
//


#include "header_cw.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
double PI;
#define PI 3.14159265

///ATTENTION: ************************************
// IN THIS FUNTION, THE LABELDATA->UNREACH KEAPS THE FACILITY INDEX. 


/* ******************************************************************************************
 * Best Fit Decreasing, a BIN PACKING HEURISTIC
 * ******************************************************************************************/

int BestFitDecreasing (double *BinList, int numveh, int TIMELIMIT,int **indexarr, int **binindex) {

  int i,j,index,tnumbin,tempindex;
  double *binsize;
  int *whichbin;
  int *indexarray;
  double temp, min;

  indexarray=*indexarr;
  whichbin=*binindex;

  /*
  printf("The unsorted binlist \n");
  for (i=0;i<numveh;i++)
    printf(" %d ",BinList[i]);
  printf("\n");
  */

  //sort nonincreasing..
  for (i=0;i<numveh-1;i++){
    for (j=i+1;j<numveh;j++){
      if (BinList[j]>BinList[i]){
	temp=BinList[i];
	BinList[i]=BinList[j];
	BinList[j]=temp;
	tempindex=indexarray[i];
	indexarray[i]=indexarray[j];
	indexarray[j]=tempindex;
  
      }
    }
  }
  /*
  printf("The sorted binlist \n");
  for (i=0;i<numveh;i++)
    printf(" %d ",BinList[i]);
  printf("\n");
  */

  //printf("TIMELIMIT=%d \n",TIMELIMIT);

  binsize=(double*)calloc (numveh,sizeof(double));
  // whichbin=(int*)calloc (numveh,sizeof(int));

  for (i=0;i<numveh;i++)
    binsize[i]=0;

  for (i=0;i<numveh;i++){
    min=INFINITY;
    index=-1;
    for (j=0;j<numveh;j++){
      if (binsize[j]+BinList[i]<=TIMELIMIT) {
	if(TIMELIMIT-binsize[j]-BinList[i]<min){
	  min=TIMELIMIT-binsize[j]-BinList[i];
	  index=j;
	}
      }
    }
    if (index!=-1){
      binsize[index]=binsize[index]+BinList[i];
      whichbin[i]=index;
    }
  }
  /*
  printf("Which bin: \n");
  for (i=0;i<numveh;i++)
    printf(" %d ", whichbin[i]);
  printf("\n");
  
  printf("bin sizes: \n");
  for (i=0;i<numveh;i++)
    printf(" %d ", binsize[i]);
  printf("\n");
  */

  tnumbin=0;
  for (i=0;i<numveh;i++){
    if(binsize[i]!=0)
      tnumbin++;
    else
      break;
  }

  *indexarr= indexarray;
  *binindex=whichbin;
  
  if (binsize)
    free(binsize);
  binsize=0;

  return(tnumbin);

}
/* ******************************************************************************************
 * symmetric clark and wright 
 * ******************************************************************************************/


double clark_wright_Alg (double **d,int ncust, int *customers, int *Demand, int TIMELIMIT, int VCap, int fac,int NCust, double VFcost, double VOcost,int *numcust, int *numcols, LABEL_NODE **ColList,int writelabellist) {

  //If writetolabellist=1, the columns are saved on a list.

  SAVINGLIST *List, *temp;
  LABEL_NODE *coltemp, *ColumnList;
  int i,j,node1,node2;
  ROUTE *RouteList, *route,*route1,*route2, *route_n1,*route_n2;
  int *unassigned;
  int found_n1, found_n2,nold,nnew;
  double time;
  //int time;
  double tvarcost,tcost,*BinList;
  int numveh;
  int  numbin;
  int *indexarray, *whichbin, *BinListCap, *BinListSize, **BinListSeq;
  int tcap,tsize,k,x,t,add;
  double ttime;
  int n_cols,n_nz;

 
    ColumnList=*ColList;
    n_cols=0;
    n_nz=0;


  if ((ncust<2) || (customers[1]==-1)){
    printf("There is only one customer for facility %d, NO Clark-Wright\n",fac);
    tcost=VFcost+2*d[fac+NCust][customers[0]]*VOcost/60;
    if(writelabellist){
      coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
      coltemp->labeldata.Cap=Demand[customers[0]];
      coltemp->labeldata.Time=d[customers[0]][NCust+fac]+d[NCust+fac][customers[0]];
      coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
      coltemp->labeldata.unreach=fac;
      coltemp->labeldata.nodeseq[customers[0]]=1;
      coltemp->next=ColumnList;
      coltemp->prev=0;
      ColumnList=coltemp;
      *ColList=ColumnList;
      coltemp=0;
      n_cols=1;
      n_nz=1;
      *numcust=n_nz;
      *numcols=n_cols;
    }
    return(tcost);
  }

  List=0;
  for (i=0;i<ncust-1;i++) {
    if (customers[i]!=-1) {
      for (j=i+1;j<ncust;j++){
	if (customers[j]!=-1) {
	  if ((Demand[customers[i]]+Demand[customers[j]]<=VCap)&&(d[customers[i]][customers[j]]<=TIMELIMIT)) {
	    temp=(SAVINGLIST *) calloc(1, sizeof(SAVINGLIST));
	    temp->n1=i; //index not customers
	    temp->n2=j;//index not customers
	    temp->saving=d[NCust+fac][customers[i]]+d[NCust+fac][customers[j]]-d[customers[i]][customers[j]];
	    temp->next=0;
	    if (!List) {
	      List=temp;
	      temp=0;
	    }
	    else {
	      temp->next=List;
	      List=temp;
	      temp=0;
	    }
	  }
	}
	else
	  break;
      }
    }
    else
      break;
  }

  /*
  temp=List;
  while (temp) {
    printf("sav=%d, n1=%d, n2=%d \n",temp->saving,temp->n1,temp->n2);
    temp=temp->next;
  }
  */

  List=listsort(List);

  /*  
  printf("Sorted List: \n");
  temp=List;
  while (temp) {
    printf("sav=%d, n1=%d, n2=%d \n",temp->saving,customers[temp->n1],customers[temp->n2]);
    temp=temp->next;
  }
  // exit(1);
  */

  unassigned=(int*)calloc (ncust,sizeof(int));
  for (i=0;i<ncust;i++)
    unassigned[i]=1;

  RouteList=0;

 
  temp=List;
  while (temp) {
    node1=temp->n1;
    node2=temp->n2;
   
    
    if ((unassigned[node1]==1) && (unassigned[node2]==1)) { //None of them are assigned 
      time=d[NCust+fac][customers[node1]]+d[customers[node1]][customers[node2]]+d[NCust+fac][customers[node2]];
      if (time<=TIMELIMIT) {
	//	printf("Both are unassigned, form a route\n");
	//construct a new route
	route=(ROUTE *) calloc (1,sizeof(ROUTE));
	route->left=customers[node1];
	route->right=customers[node2];
	route->size=2;
	route->cap=Demand[customers[node1]]+Demand[customers[node2]];
	route->time=d[NCust+fac][customers[node1]]+d[customers[node1]][customers[node2]]+d[NCust+fac][customers[node2]];
	route->next=0;
	route->prev=0;
	route->seq=(int*)calloc(route->size,sizeof(int));
	route->seq[0]=customers[node1];
	route->seq[1]=customers[node2];
	route->next= RouteList;
	if (RouteList)
	  RouteList->prev=route;
	RouteList=route;
	route=0;
	unassigned[node1]=0;
	unassigned[node2]=0;
      }
      //else
      //	printf("TIMELIMIT is exceeded, time for route is %lf \n",time);
    }
    else if (unassigned[node1]+unassigned[node2]==1) { //one of them is assigned
      if (unassigned[node1]==0) {
	nold=node1;
	nnew=node2;
      }
      else {
	nold=node2;
	nnew=node1;
      }
      // printf("Node %d is unassigned, node %d is assigned\n",customers[nnew],customers[nold]);
      route=RouteList;
      while (route) {
	if ((route->left==customers[nold])||(route->right==customers[nold])){
	  time=route->time-d[customers[nold]][NCust+fac]+d[customers[nold]][customers[nnew]]+d[NCust+fac][customers[nnew]];
	    if ((route->cap + Demand[customers[nnew]] <=VCap) && (time<=TIMELIMIT)){
	      //add nnew to the existing route
	      unassigned[nnew]=0;
	      route1=(ROUTE *) calloc (1,sizeof(ROUTE));
	      route1->cap=route->cap + Demand[customers[nnew]];
	      route1->time=time;
	      route1->size= route->size+1;
	      if (route->left==customers[nold]){
		route1->left=customers[nnew];
		route1->right=route->right;
		route1->seq=(int*)calloc(route1->size,sizeof(int));
		route1->seq[0]=customers[nnew];
		for(i=1;i<route1->size;i++)
		  route1->seq[i]=route->seq[i-1];
	      } //if connect from left
	      else {
		route1->right=customers[nnew];
		route1->left=route->left;
		route1->seq=(int*)calloc(route1->size,sizeof(int));
		route1->seq[route1->size-1]=customers[nnew];
		for(i=0;i<route1->size-1;i++)
		  route1->seq[i]=route->seq[i];
	      } //if connect from left
	      route1->next=0;
	      route1->prev=0;
	      route1->next= RouteList;
	      if (RouteList)
		RouteList->prev=route1;
	      RouteList=route1;
	     
	      //delete route
	      (route->prev)->next=route->next;
	      if (route->next)
		(route->next)->prev=route->prev;
	      route->next=0;
	      route->prev=0;
	      free(route->seq);
	      free(route);
	      route=0;
	      break;
	    }
	  break;
	}
	else 
	  route=route->next;
      } //while route
    } //(unassigned[node1]+unassigned[node2]==1) { //one of them is assigned

    else if (unassigned[node1]+unassigned[node2]==0) { //both are assigned
      //printf("Node %d is assigned, node %d is assigned\n",customers[node1],customers[node2]);
      route=RouteList;
      found_n1=0;
      found_n2=0;
      route_n1=0;
      route_n2=0;
      
      while (route) {
	if ((route->left==customers[node1])||(route->right==customers[node1])) {
	  route_n1=route;
	  found_n1=1;
	}
	if ((route->left==customers[node2])||(route->right==customers[node2])) {
	  route_n2=route;
	  found_n2=1;
	}
	if ((found_n1)&&(found_n2)) 
	  break;
	else
	  route=route->next;
      } //while
      if (((found_n1)&&(found_n2)) && (route_n1!=route_n2)) {
	/*
	 * CASE1:[n1...]  [...n2]
	 */
	if ((route_n1->left==customers[node1]) && (route_n2->right==customers[node2])){
	  time=route_n1->time+route_n2->time+d[customers[node2]][customers[node1]]-d[NCust+fac][customers[node1]]-d[customers[node2]][NCust+fac];
	  if ((route_n1->cap+route_n2->cap<=VCap)&&(time<=TIMELIMIT)) {
	    //combine route and route 1
	    route2=(ROUTE *) calloc (1,sizeof(ROUTE));
	    route2->cap=route_n1->cap + route_n2->cap;
	    route2->time=time;
	    route2->size= route_n1->size+route_n2->size;
	    route2->right=route_n1->right;
	    route2->left=route_n2->left;
	    route2->seq=(int*)calloc(route2->size,sizeof(int));
	    for(i=0;i<route_n2->size;i++)
	      route2->seq[i]=route_n2->seq[i];
	    for(j=0;j<route_n1->size;j++)
	      route2->seq[i+j]=route_n1->seq[j];
	    route2->next=0;
	    route2->prev=0;
	    route2->next= RouteList;
	    if (RouteList)
	      RouteList->prev=route2;
	    RouteList=route2;
	    route2=0;

	    //delete route_n1 and route_n2
	    (route_n1->prev)->next=route_n1->next;
	    if (route_n1->next)
	      (route_n1->next)->prev=route_n1->prev;
	    route_n1->next=0;
	    route_n1->prev=0;
	    free(route_n1->seq);
	    free(route_n1);
	    route_n1=0;

	    (route_n2->prev)->next=route_n2->next;
	    if (route_n2->next)
	      (route_n2->next)->prev=route_n2->prev;
	    route_n2->next=0;
	    route_n2->prev=0;
	    free(route_n2->seq);
	    free(route_n2);
	    route_n2=0;
	  } //if feasible
	} //if 
	
	/*
	 * CASE2:[...n1]  [n2...]
	 */
	
	else if ((route_n1->right==customers[node1]) && (route_n2->left==customers[node2])){
	  time=route_n1->time+route_n2->time+d[customers[node1]][customers[node2]]-d[NCust+fac][customers[node2]]-d[customers[node1]][NCust+fac];
	  if ((route_n1->cap+route_n2->cap<=VCap)&&(time<=TIMELIMIT)) {
	    //combine route and route 1
	    route2=(ROUTE *) calloc (1,sizeof(ROUTE));
	    route2->cap=route_n1->cap + route_n2->cap;
	    route2->time=time;
	    route2->size= route_n1->size+route_n2->size;
	    route2->right=route_n2->right;
	    route2->left=route_n1->left;
	    route2->seq=(int*)calloc(route2->size,sizeof(int));
	    for(i=0;i<route_n1->size;i++)
	      route2->seq[i]=route_n1->seq[i];
	    for(j=0;j<route_n2->size;j++)
	      route2->seq[i+j]=route_n2->seq[j];
	    route2->next=0;
	    route2->prev=0;
	    route2->next= RouteList;
	    if (RouteList)
	      RouteList->prev=route2;
	    RouteList=route2;
	    
	    //delete route_n1 and route_n2
	    (route_n1->prev)->next=route_n1->next;
	    if (route_n1->next)
	      (route_n1->next)->prev=route_n1->prev;
	    route_n1->next=0;
	    route_n1->prev=0;
	    free(route_n1->seq);
	    free(route_n1);
	    route_n1=0;

	    (route_n2->prev)->next=route_n2->next;
	    if (route_n2->next)
	      (route_n2->next)->prev=route_n2->prev;
	    route_n2->next=0;
	    route_n2->prev=0;
	    free(route_n2->seq);
	    free(route_n2);
	    route_n2=0;
	  } //if feasible
	} //else if 

	/*
	 * CASE3:[...n1]  [...n2]
	 */

	else if ((route_n1->right==customers[node1]) && (route_n2->right==customers[node2])){
	  time=route_n1->time+route_n2->time+d[customers[node1]][customers[node2]]-d[customers[node2]][NCust+fac]-d[customers[node1]][NCust+fac];
	  if ((route_n1->cap+route_n2->cap<=VCap)&&(time<=TIMELIMIT)) {
	    //combine route and route 1
	    route2=(ROUTE *) calloc (1,sizeof(ROUTE));
	    route2->cap=route_n1->cap + route_n2->cap;
	    route2->time=time;
	    route2->size= route_n1->size+route_n2->size;
	    route2->right=route_n2->left;
	    route2->left=route_n1->left;
	    route2->seq=(int*)calloc(route2->size,sizeof(int));
	    for(i=0;i<route_n1->size;i++)
	      route2->seq[i]=route_n1->seq[i];
	    for(j=0;j<route_n2->size;j++)
	      route2->seq[i+j]=route_n2->seq[route_n2->size-1-j];
	    route2->next=0;
	    route2->prev=0;
	    route2->next= RouteList;
	    if (RouteList)
	      RouteList->prev=route2;
	    RouteList=route2;
	    
	    //delete route_n1 and route_n2
	    (route_n1->prev)->next=route_n1->next;
	    if (route_n1->next)
	      (route_n1->next)->prev=route_n1->prev;
	    route_n1->next=0;
	    route_n1->prev=0;
	    free(route_n1->seq);
	    free(route_n1);
	    route_n1=0;

	    (route_n2->prev)->next=route_n2->next;
	    if (route_n2->next)
	      (route_n2->next)->prev=route_n2->prev;
	    route_n2->next=0;
	    route_n2->prev=0;
	    free(route_n2->seq);
	    free(route_n2);
	    route_n2=0;
	  } //if feasible
	} //else if 

	/*
	 * CASE4:[n1...]  [n2...]
	 */

	else if ((route_n1->left==customers[node1]) && (route_n2->left==customers[node2])){
	  time=route_n1->time+route_n2->time+d[customers[node1]][customers[node2]]-d[NCust+fac][customers[node2]]-d[NCust+fac][customers[node1]];
	  if ((route_n1->cap+route_n2->cap<=VCap)&&(time<=TIMELIMIT)) {
	    //combine route and route 1
	    route2=(ROUTE *) calloc (1,sizeof(ROUTE));
	    route2->cap=route_n1->cap + route_n2->cap;
	    route2->time=time;
	    route2->size= route_n1->size+route_n2->size;
	    route2->right=route_n2->right;
	    route2->left=route_n1->right;
	    route2->seq=(int*)calloc(route2->size,sizeof(int));
	    for(i=0;i<route_n1->size;i++)
	      route2->seq[i]=route_n1->seq[route_n1->size-1-i];
	    for(j=0;j<route_n2->size;j++)
	      route2->seq[i+j]=route_n2->seq[j];
	    route2->next=0;
	    route2->prev=0;
	    route2->next= RouteList;
	    if (RouteList)
	      RouteList->prev=route2;
	    RouteList=route2;
	    
	    //delete route_n1 and route_n2
	    (route_n1->prev)->next=route_n1->next;
	    if (route_n1->next)
	      (route_n1->next)->prev=route_n1->prev;
	    route_n1->next=0;
	    route_n1->prev=0;
	    free(route_n1->seq);
	    free(route_n1);
	    route_n1=0;

	    (route_n2->prev)->next=route_n2->next;
	    if (route_n2->next)
	      (route_n2->next)->prev=route_n2->prev;
	    route_n2->next=0;
	    route_n2->prev=0;
	    free(route_n2->seq);
	    free(route_n2);
	    route_n2=0;
	  } //if feasible
	} //else if 




      } //if route_n1, route_n2 are found
    }//if both are assigned

    temp=temp->next;
  } //while temp
  
  route=RouteList;
  tvarcost=0;
  numveh=0;
 
  if(writelabellist)
    printf("Routes are: \n");
  // printf("The routes =");
  while (route) {
    numveh++;
    tvarcost=route->time*VOcost/60+tvarcost;
    if (writelabellist){
      coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
      coltemp->labeldata.Cap=route->cap;
      coltemp->labeldata.Time=route->time;
      coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
      coltemp->labeldata.unreach=fac;
      printf("[%d - ",fac+NCust);
      for (i=0;i<route->size;i++){
	printf("%d -",route->seq[i]);
	coltemp->labeldata.nodeseq[route->seq[i]]=i+1;
      }
      
      printf(" %d]\n ",fac+NCust);
    
      coltemp->next=ColumnList;
      if (ColumnList)
	ColumnList->prev=coltemp;
      coltemp->prev=0;
      ColumnList=coltemp;
      coltemp=0;
      n_cols++;
      n_nz=n_nz+route->size;
    }
 
    route=route->next;
  }
  // printf("\n");

  //  printf("unassigned customers:[");
  for (i=0;i<ncust;i++){
    if (customers[i]!=-1) {
      if (unassigned[i]) {
	if (writelabellist)
	  printf("[%d - %d - %d]\n",fac+NCust,customers[i],fac+NCust);
	//printf(" %d %d ",customers[i],fac+NCust);
	numveh++;
	tvarcost=tvarcost+ (double) 2*d[fac+NCust][customers[i]]*VOcost/60;
	
      }
    }
    else 
      break;
  }
  // printf("\n");

  //printf("]\n");
  //printf("number of paths is %d \n",numveh);
  
  BinList=(double *)calloc (numveh, sizeof(double));
  indexarray=(int *)calloc (numveh, sizeof(int));
  whichbin=(int *)calloc (numveh, sizeof(int));

  BinListCap=(int *)calloc (numveh, sizeof(int));
  BinListSize=(int *)calloc (numveh, sizeof(int));
  BinListSeq=(int **)calloc (numveh, sizeof(int*));


 
  route=RouteList;
  i=0;
  while (route) {
    BinList[i]=route->time;
    BinListCap[i]=route->cap;
    BinListSize[i]=route->size;
    BinListSeq[i]=(int *)calloc (BinListSize[i], sizeof(int));
    for (j=0;j<route->size;j++) 
      BinListSeq[i][j]=route->seq[j];
    route=route->next;
    i++;
  }

  for (j=0;j<ncust;j++){
    if (customers[j]!=-1) {
      if (unassigned[j]) {
	BinList[i]=2*d[fac+NCust][customers[j]];
	BinListCap[i]=Demand[customers[j]];
	BinListSize[i]=1;
	BinListSeq[i]=(int *)calloc (BinListSize[i], sizeof(int));
	BinListSeq[i][0]=customers[j];
	i++;
      }
    }
    else
      break;
  }

  /*
  for (i=0;i<numveh;i++)
    printf(" %d ",BinList[i]);
  printf("\n");
  */

  for (i=0;i<numveh;i++) 
    indexarray[i]=i;

  numbin= BestFitDecreasing (BinList,numveh,TIMELIMIT,&indexarray,&whichbin ) ;
  /*
  printf("index array:\n");
  for (i=0;i<numveh;i++)
    printf(" %d ",indexarray[i]);
  printf("\n");
  */
  /*
  printf("which bin array:\n");
  for (i=0;i<numveh;i++)
    printf(" %d ",whichbin[i]);
  printf("\n");
  */
  if (writelabellist) {
    
    printf("which routes are combined: ");
    for (i=0;i<numveh;i++)
      printf(" %d in pairing %d, ",i, whichbin[i]);
    printf("\n");
  }

  if (writelabellist){
    
    x=0;

    for(i=0;i<numbin;i++){
     
      ttime=0;
      tcap=0;
      tsize=0;
      add=0;
      for (j=0;j<numveh;j++){
	if (whichbin[j]==i) {
	  add++;
	  ttime=ttime+BinList[j];
	  tcap=tcap+BinListCap[indexarray[j]];
	  tsize=tsize+BinListSize[indexarray[j]];
	  
	}
      }
      if (add>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=tcap;
	coltemp->labeldata.Time=ttime;
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
     
      
	x=x+tsize;
	
	t=1;
	for (j=0;j<numveh;j++){
	  if (whichbin[j]==i) {
	    for (k=0;k<BinListSize[indexarray[j]];k++){
	      coltemp->labeldata.nodeseq[BinListSeq[indexarray[j]][k]]=t;
	      t++;
	    }
	    t++;
	  }
	}	
	
	coltemp->next=ColumnList;
	coltemp->prev=0;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;
      }
    }
    n_nz=n_nz+x;
    *numcust=n_nz;
    *numcols=n_cols;
    *ColList=ColumnList;
    
  }

      	  
  ///////////////////////////////
  temp=List;
  while (temp) {
    List=temp->next;
    free(temp);
    temp=List;
  }  
  //free routelist
  route=RouteList;
  while (route) {
    RouteList=route->next;
    free(route->seq);
    if (RouteList)
      RouteList->prev=0;
    route->next=0;
    route->prev=0;
    free(route);
    route=RouteList;
  }
  RouteList=0;
  ColumnList=0;

  free(BinList);
  free(indexarray);
  free(whichbin);
  free(BinListCap);
  free(BinListSize);
  
  for (i=0;i<numveh;i++){  
    free(BinListSeq[i]);
    BinListSeq[i]=0;
  }
  free(BinListSeq);
  BinListSeq=0;
  BinList=0;
  BinListCap=0;
  BinListSize=0;
  indexarray=0;
  whichbin=0;

  free(unassigned);
  unassigned=0;

  tcost=(double)numbin*VFcost+tvarcost;

  return(tcost);   
}

/*
 * **************************************************************************************************************
 */



int assignmentOfCust(double **d,int *Open, int ***CustAsgn, int FCap, int *Demand, int NFac,int NCust,int TIMELIMIT){
  /*
   * Assigns customers to open facilities. Choses closest facility. But also considers timelimit and facility capacity
   * If the assignment of customers to open facilities is infeasible, the function returns 0. Otherwise returns 1.
   */
  int **assign,j;
  int *TCap;
  int i, index;
  int *countfac,feas;
  double min;

  feas=1;
  
  /*
  printf("FCap=%d,NFac=%d,NCust=%d \n",FCap,NFac,NCust);
  for (i=0;i<NFac;i++)
    printf("Open[%d]=%d \n",i,Open[i]);
  */


  assign=*CustAsgn;
  for (i=0;i<NFac;i++){ 
    for (j=0;j<NCust+1;j++) 
      assign[i][j]=0;
  }


  TCap=(int *)calloc (NFac,sizeof(int));
  countfac=(int *)calloc (NFac,sizeof(int));

  for (i=0;i<NFac;i++){ 
    TCap[i]=0;
    countfac[i]=0;
  }
 
  
  for (j=0;j<NCust;j++) {
    index=-1;
    min=INFINITY;
    for (i=0;i<NFac;i++){
      if ((Open[i]) && (TCap[i]+Demand[j]<=FCap)){
	if ((d[j][i+NCust]<min) && (2*d[j][i+NCust]<=TIMELIMIT)) {
	  min=d[j][i+NCust];
	  index = i;
	}
      }
    }
    // printf("index=%d, j=%d \n",index,j);
    if (index==-1) {
      feas=0;
      break;
    }
    else {
      TCap[index]=TCap[index]+Demand[j];
      assign[index][countfac[index]]=j;
      countfac[index]++;
     
    }
 
  }//for
  if (feas!=0) {
    for (i=0;i<NFac;i++){
      //if(countfac[i]!=0)
	assign[i][countfac[i]]=-1;
    }
  }

  /*
  if (feas){
    for (j=0;j<NFac;j++){
      if (Open[j]==1){
	printf("The customers assigned to facility %d (with total capacity %d) are:[ ",j,TCap[j]);
	for (i=0;i<NCust;i++){
	  if (assign[j][i]!=-1)
	    printf(" %d ",assign[j][i]);
	  else 
	    break;
	}
	printf("\n");
      }
    }
  }
  */


  free(TCap);
  TCap=0;
  free(countfac);
  countfac=0;

  *CustAsgn=assign;
  assign=0;

  return(feas);

}



/* ************************************************************************************************************
 * NEAREST NEIGHBOUR HEURISTIC, TSP HEURISTIC IS UPDATED FOR VRP
 * *************************************************************************************************************/

double NearestNeighbor (double **d, int *customers, int *Demand, int TIMELIMIT, int VCap, int fac,int NCust, double VFcost, double VOcost,int *numcust, int *numcols, LABEL_NODE **ColList,int writelabellist){

  int *Visited,*indexarray,*whichbin;
  int i,p1,p2,index,count,unvisited,numveh,numbin,j;
  int Tdemand;
  int *RouteSeq;
  double tvarcost,tcost,*BinList, min, ttime, Tdist;
  LABEL_NODE *ColumnList,*coltemp;
  int *BinListCap, *BinListSize,**BinListSeq;
  int tcap,tsize,k,x,t;
  //  double ttime;
  // int ttime;
  int n_cols,n_nz,add;


  Visited=(int *)calloc(NCust,sizeof(int));
  RouteSeq=(int*)calloc(2*NCust,sizeof(int));
 
  for (i=0;i<NCust;i++)
    Visited[i]=1;
  
  for (i=0;i<NCust;i++) {
    if (customers[i]!=-1)
      Visited[customers[i]]=0;
    else
      break;
  }

  p1=fac+NCust;
  Tdemand=0;
  Tdist=0;
  unvisited=1;
  count=0;
  
  numveh=0;

  //printf("p1=%d\n",p1);

  while (unvisited==1) {
    
    min=INFINITY;
    index=-1;
    
    for (i=0;i<NCust;i++){
      if ((Visited[i]==0)&&(d[p1][i]<min)&&(Tdemand+Demand[i]<=VCap)&&(Tdist+d[p1][i]+d[i][fac+NCust]<=TIMELIMIT)) {
	index=i;
	min=d[p1][i];
      }
    }
    //printf("index=%d \n",index);

    if (index!=-1) { //means a nearest feasible customer node is found 
      p2=index;
      Visited[p2]=1;
      Tdemand=Tdemand+Demand[p2];
      Tdist=Tdist+d[p1][p2];
      RouteSeq[count]=p2;
      count++;
      p1=p2;


    }
    else { //RETURN TO DEPOT
      Tdist=0;
      Tdemand=0;
      //  printf("count=%d \n",count);
      RouteSeq[count]=fac+NCust;
      count++;
      p1=fac+NCust;
      numveh++;
    }

    //  printf("p1=%d\n",p1);

    unvisited=0;
    for (i=0;i<NCust;i++){
      if (Visited[i]==0) {
	unvisited=1;
	break;
      }
    }
  } //while unvisited==1

  if (RouteSeq[count-1]<NCust){
    RouteSeq[count]=fac+NCust;
    count++;
    numveh++;
  }

  if (writelabellist){
    printf("Routes are [ %d ",NCust+fac);
    for (i=0;i<count;i++)
      printf("%d ",RouteSeq[i]);
    //printf("...numveh=%d\n",numveh);
    printf("\n");
  }

  
  BinList=(double *)calloc (numveh, sizeof(double));
  indexarray=(int *)calloc (numveh, sizeof(int));
  whichbin=(int *)calloc (numveh, sizeof(int));
  
  BinListCap=(int *)calloc (numveh, sizeof(int));
  BinListSize=(int *)calloc (numveh, sizeof(int));
  BinListSeq=(int **)calloc (numveh, sizeof(int*));

 

  i=0;
  BinList[i]=d[fac+NCust][RouteSeq[0]];
  BinListCap[i]=Demand[RouteSeq[0]];
  BinListSize[i]=0;
  BinListSeq[i]=(int *)calloc (NCust, sizeof(int));
  BinListSeq[i][BinListSize[i]]=RouteSeq[0];
  BinListSize[i]++;

  for (j=0;j<count-1;j++){
    BinList[i]=BinList[i]+d[RouteSeq[j]][RouteSeq[j+1]];
    if (RouteSeq[j+1]<NCust){
      BinListCap[i]=BinListCap[i]+Demand[RouteSeq[j+1]];
      BinListSeq[i][BinListSize[i]]=RouteSeq[j+1];
      BinListSize[i]++;
    }
    if (RouteSeq[j+1]>=NCust) {
      i++;
      if (i<numveh) {
	BinListCap[i]=0;
	BinListSize[i]=0;
	BinListSeq[i]=(int *)calloc (NCust, sizeof(int));
      
      }
    }
  }
  
  tvarcost=0;

  for (i=0;i<numveh;i++) {
    //printf(" %d ",BinList[i]);
    tvarcost=tvarcost+(double)BinList[i]*VOcost/60;
  }
  // printf("\n");
  
  
  ColumnList=*ColList;
  n_cols=0;
  n_nz=0;
  
  if (writelabellist){
    for (i=0;i<numveh;i++) {
      if (BinListSize[i]>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=BinListCap[i];
	coltemp->labeldata.Time=BinList[i];
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
	
	for (j=0;j<BinListSize[i];j++){
	  coltemp->labeldata.nodeseq[BinListSeq[i][j]]=j+1;
	}
	
	coltemp->next=ColumnList;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	coltemp->prev=0;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;
	n_nz=n_nz+BinListSize[i];
      } //if (BinListSize[i]>1){
    }
  }



  for (i=0;i<numveh;i++) 
    indexarray[i]=i;
  
  numbin= BestFitDecreasing (BinList,numveh,TIMELIMIT,&indexarray,&whichbin ) ;
  /*
  printf("index array:\n");
  for (i=0;i<numveh;i++)
    printf(" %d ",indexarray[i]);
  printf("\n");
  */

  if (writelabellist){
    printf("Routes combined: ");
    for (i=0;i<numveh;i++)
      printf(" %d in pairing %d, ",i,whichbin[i]);
    printf("\n");
  }



 if (writelabellist){  
    
    x=0;

    for(i=0;i<numbin;i++){
      
      ttime=0;
      tcap=0;
      tsize=0;
      add=0;

      for (j=0;j<numveh;j++){
	if (whichbin[j]==i) {
	  ttime=ttime+BinList[j];
	  tcap=tcap+BinListCap[indexarray[j]];
	  tsize=tsize+BinListSize[indexarray[j]];
	  add++;
	}
      }
      if (add>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=tcap;
	coltemp->labeldata.Time=ttime;
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
	
	
	x=x+tsize;
	
	t=1;
	for (j=0;j<numveh;j++){
	  if (whichbin[j]==i) {
	    for (k=0;k<BinListSize[indexarray[j]];k++){
	      coltemp->labeldata.nodeseq[BinListSeq[indexarray[j]][k]]=t;
	      t++;
	    }
	    t++;
	  }
	}	
	
	coltemp->next=ColumnList;
	coltemp->prev=0;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;

      }
    }
    n_nz=n_nz+x;
    *numcust=n_nz;
    *numcols=n_cols;
    *ColList=ColumnList;
 }


 ColumnList=0;
 free(Visited);
 free(RouteSeq);
 Visited=0;
 RouteSeq=0;


 free(BinList);
 free(indexarray);
 free(whichbin);
 free(BinListCap);
 free(BinListSize);

 for (i=0;i<numveh;i++){  
   free(BinListSeq[i]);
   BinListSeq[i]=0;
 }
 free(BinListSeq);
 BinListSeq=0;
 BinList=0;
 BinListCap=0;
 BinListSize=0;
 indexarray=0;
 whichbin=0;



  tcost=(double) numbin*VFcost+tvarcost;

  return(tcost);

} 

/*
 * *********************************************************
 */

double SweepHeuristic (double **d,  double *X, double *Y,int *customers, int *Demand, int TIMELIMIT, int VCap, int fac,int NCust, double VFcost, double VOcost,int *numcust, int *numcols, LABEL_NODE **ColList,int writelabellist){
  
  double *ArcTan,temp;
  int i,tempindex,j,count,numveh;
  int *sortedindexarray, *RouteSeq,*indexarray,*whichbin;;
  int Tdemand,p1,p2;
  double tvarcost,tcost, Tdist, *BinList;
  int numbin;
  LABEL_NODE *ColumnList,*coltemp;
  int *BinListCap, *BinListSize,**BinListSeq;
  int tcap,tsize,k,x,t;
  double ttime;
  //int ttime;
  int n_cols,n_nz,add;


  ArcTan=(double *) calloc (NCust,sizeof(double));
  sortedindexarray=(int *)calloc(NCust,sizeof(int));
  RouteSeq=(int*)calloc(2*NCust,sizeof(int));

  count=0;
  numveh=0;

  //Calculate the angle 
  for (i=0;i<NCust;i++){
    if (customers[i]!=-1){
      sortedindexarray[i]=i;
      if (((Y[customers[i]]-Y[NCust+fac])>=0)&&((X[customers[i]]-X[NCust+fac])>=0))  //region I
	ArcTan[i]=180/PI*atan((Y[customers[i]]-Y[NCust+fac])/(X[customers[i]]-X[NCust+fac]));
      else if (((Y[customers[i]]-Y[NCust+fac])>=0)&&((X[customers[i]]-X[NCust+fac])<0))  //region II
	ArcTan[i]=180/PI*atan((Y[customers[i]]-Y[NCust+fac])/(X[customers[i]]-X[NCust+fac]))+180;
      else if (((Y[customers[i]]-Y[NCust+fac])<0)&&((X[customers[i]]-X[NCust+fac])<0))  //region III
	ArcTan[i]=180/PI*atan((Y[customers[i]]-Y[NCust+fac])/(X[customers[i]]-X[NCust+fac]))+180;
      else if (((Y[customers[i]]-Y[NCust+fac])<0)&&((X[customers[i]]-X[NCust+fac])>=0))  //region IV
	ArcTan[i]=180/PI*atan((Y[customers[i]]-Y[NCust+fac])/(X[customers[i]]-X[NCust+fac]))+360;
    }
    else { 
      ArcTan[i]=INFINITY;
      sortedindexarray[i]=-1;
      break;
    }
  }
  /*
  for (i=0;i<NCust;i++){
    if (customers[i]!=-1)
      printf("ArcTan[%d]=%lf \n",i, ArcTan[i]);
    else
      break;
  }
  */
  /******************************************************************************/  
  /*
  printf("Unsorted indices: ");
  for (i=0;i<NCust;i++){
    if (customers[i]!=-1)
      printf("%d  ", sortedindexarray[i]);
    else
      break;
  }
  printf("\n");
  */  
  //sort the angles nondecreasing..
  for (i=0;i<NCust-1;i++){
    if (customers[i]!=-1) {
      for (j=i+1;j<NCust;j++){
	if (customers[j]!=-1) {
	  if ((ArcTan[j]<ArcTan[i]) || ((ArcTan[j]==ArcTan[i])&&(d[NCust+fac][customers[j]]<d[NCust+fac][customers[i]]))){
	    temp=ArcTan[i];
	    ArcTan[i]=ArcTan[j];
	    ArcTan[j]=temp;
	    tempindex=sortedindexarray[i];
	    sortedindexarray[i]=sortedindexarray[j];
	    sortedindexarray[j]=tempindex;
	    
	  } //if
	}
	else 
	  break;
      }//for j
    }//if
    else
      break;
  }//for i
  /*
  printf("Sorted indices: ");
  for (i=0;i<NCust;i++){
    if (customers[i]!=-1)
      printf("%d  ", sortedindexarray[i]);
    else
      break;
  }
  printf("\n");
  */

 /******************************************************************************/  

  Tdemand=0;
  Tdist=0;
  p1=fac+NCust;
  
  for (i=0;i<NCust;i++){
    if (customers[i]!=-1){
      tempindex=customers[sortedindexarray[i]];
      if ((Tdemand+Demand[tempindex]<=VCap)&&(Tdist+d[p1][tempindex]+d[tempindex][fac+NCust]<=TIMELIMIT)) { //feasible
	
	p2=customers[sortedindexarray[i]];
	Tdemand=Tdemand+Demand[p2];
	Tdist=Tdist+d[p1][p2];
	RouteSeq[count]=p2;
	count++;
	p1=p2;
      }
      
      else { //RETURN TO DEPOT
	RouteSeq[count]=fac+NCust;
	count++;
	p1=fac+NCust;
	numveh++;
	p2=tempindex;
	Tdist=d[p1][p2];
	Tdemand=Demand[p2];
	RouteSeq[count]=p2;
	count++;
	p1=p2;	
      }
    } //if customers!=-1
    else
      break;
  } //for

  RouteSeq[count]=fac+NCust;
  count++;
  numveh++;
  
  if (writelabellist){
    printf("Routes are [ %d ",NCust+fac);
    for (i=0;i<count;i++)
      printf("%d ",RouteSeq[i]);
    //printf("...numveh=%d\n",numveh);
    printf("\n");
  }

  BinList=(double *)calloc (numveh, sizeof(double));
  indexarray=(int *)calloc (numveh, sizeof(int));
  whichbin=(int *)calloc (numveh, sizeof(int));
  
  BinListCap=(int *)calloc (numveh, sizeof(int));
  BinListSize=(int *)calloc (numveh, sizeof(int));
  BinListSeq=(int **)calloc (numveh, sizeof(int*));


  i=0;
  BinList[i]=d[fac+NCust][RouteSeq[0]];
  BinListCap[i]=Demand[RouteSeq[0]];
  BinListSize[i]=0;
  BinListSeq[i]=(int *)calloc (NCust, sizeof(int));
  BinListSeq[i][BinListSize[i]]=RouteSeq[0];
  BinListSize[i]++;

  for (j=0;j<count-1;j++){
    BinList[i]=BinList[i]+d[RouteSeq[j]][RouteSeq[j+1]];
    if (RouteSeq[j+1]<NCust){
      BinListCap[i]=BinListCap[i]+Demand[RouteSeq[j+1]];
      BinListSeq[i][BinListSize[i]]=RouteSeq[j+1];
      BinListSize[i]++;
    }

    if (RouteSeq[j+1]>=NCust){
      i++;
      if (i<numveh) {
	BinListCap[i]=0;
	BinListSize[i]=0;
	BinListSeq[i]=(int *)calloc (NCust, sizeof(int));
	
      }
    }
  } //for j
  
  tvarcost=0;
  
  for (i=0;i<numveh;i++) {
    //printf(" %d ",BinList[i]);
    tvarcost=tvarcost+(double)BinList[i]*VOcost/60;
  }
  // printf("\n");
  
  ColumnList=*ColList;
  n_cols=0;
  n_nz=0;
  
  if (writelabellist){
    for (i=0;i<numveh;i++) {
      if (BinListSize[i]>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=BinListCap[i];
	coltemp->labeldata.Time=BinList[i];
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
	
	for (j=0;j<BinListSize[i];j++){
	  coltemp->labeldata.nodeseq[BinListSeq[i][j]]=j+1;
	}
	
	coltemp->next=ColumnList;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	coltemp->prev=0;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;
	n_nz=n_nz+BinListSize[i];
      } //if (BinListSize[i]>1)
    }
  }
  


  
  for (i=0;i<numveh;i++) 
    indexarray[i]=i;
  
  numbin= BestFitDecreasing (BinList,numveh,TIMELIMIT,&indexarray,&whichbin ) ;
  
  /*
  printf("index array:\n");
  for (i=0;i<numveh;i++)
    printf(" %d ",indexarray[i]);
  printf("\n");
  */

  if (writelabellist){
    printf("Routes combined: ");
    for (i=0;i<numveh;i++)
      printf(" %d in pairing %d, ",i,whichbin[i]);
    printf("\n");
  }


  if (writelabellist){  
    
    x=0;
    
    for(i=0;i<numbin;i++){
      
      ttime=0;
      tcap=0;
      tsize=0;
      add=0;

      for (j=0;j<numveh;j++){
	if (whichbin[j]==i) {
	  ttime=ttime+BinList[j];
	  tcap=tcap+BinListCap[indexarray[j]];
	  tsize=tsize+BinListSize[indexarray[j]];
	  add++;	  
	}
      }
      if (add>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=tcap;
	coltemp->labeldata.Time=ttime;
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
	
      
	x=x+tsize;
	
	t=1;
	for (j=0;j<numveh;j++){
	  if (whichbin[j]==i) {
	    for (k=0;k<BinListSize[indexarray[j]];k++){
	      coltemp->labeldata.nodeseq[BinListSeq[indexarray[j]][k]]=t;
	      t++;
	    }
	    t++;
	  }
	}	
	
	coltemp->next=ColumnList;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	coltemp->prev=0;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;

      }
    }
    n_nz=n_nz+x;
    *numcust=n_nz;
    *numcols=n_cols;
    *ColList=ColumnList;
  }


  ColumnList=0;
  free(RouteSeq);
  RouteSeq=0;
  free(ArcTan);
  free(sortedindexarray);
  ArcTan=0;
  sortedindexarray=0;
  
  free(BinList);
  free(indexarray);
  free(whichbin);
  free(BinListCap);
  free(BinListSize);
  
  for (i=0;i<numveh;i++){  
    free(BinListSeq[i]);
    BinListSeq[i]=0;
  }
  free(BinListSeq);
  BinListSeq=0;
  BinList=0;
  BinListCap=0;
  BinListSize=0;
  indexarray=0;
  whichbin=0;
  
  
  

  tcost=(double)numbin*VFcost+tvarcost;
  
  return(tcost);
  
}

/*
 * *********************************************************
 */

double NearestInsertionHeuristic (double **d, int *customers, int *Demand, int TIMELIMIT, int VCap, int fac,int NCust, double VFcost, double VOcost, int *numcust, int *numcols, LABEL_NODE **ColList,int writelabellist){

  int *Visited,*indexarray,*whichbin;
  int i,p1,p2,indexcust, indexplace, count,unvisited,numveh,numbin,j;
  int Tdemand;
  double Tdist;
  int *RouteSeq, *route;
  double tvarcost,tcost;
  LABEL_NODE *ColumnList,*coltemp;
  int *BinListCap, *BinListSize,**BinListSeq;
  int tcap,tsize,k,x,t;
  //double ttime;
  double ttime,min;
  int n_cols,n_nz,add;
  double *BinList;



  Visited=(int *)calloc(NCust,sizeof(int));
  RouteSeq=(int*)calloc(2*NCust,sizeof(int));
  route=(int*)calloc(NCust+2,sizeof(int));
 

  for (i=0;i<NCust;i++)
    Visited[i]=1;
  
  for (i=0;i<NCust;i++) {
    if (customers[i]!=-1)
      Visited[customers[i]]=0;
    else
      break;
  }

  /*
  printf ("Unvisited customers:");
  for (i=0;i<NCust;i++) {
    if (Visited[i]==0)
      printf("%d ",i);
  }
  printf("\n");
  */

  unvisited=1;
  count=0;
  
  numveh=0;
  
  //initialize a route:  ////////////////
  route[0]=NCust+fac;
  route[1]=customers[0];
  route[2]=NCust+fac;
  route[3]=-1;
  Tdemand=Demand[customers[0]];
  Tdist=d[route[0]][route[1]]+d[route[1]][route[2]];
  Visited[customers[0]]=1;
  //////////////////////////////////////

  /*
  printf("The route is="); 
  for (i=0;i<NCust+2;i++){
    if (route[i]!=-1)
      printf("%d ",route[i]);
    else 
      break;
  }
  printf("\n");
  */

  while (unvisited==1) {

    min=INFINITY;
    indexcust=-1;
    indexplace=-1;

    for (i=0;i<NCust;i++){
      if ((Visited[i]==0)&&(Tdemand+Demand[i]<=VCap)) {
	  for (j=0;j<NCust+1;j++){
	    if ((route[j]!=-1)&&(route[j+1]!=-1)) {
	      if (((d[route[j]][i]+d[i][route[j+1]]-d[route[j]][route[j+1]]) < min)&&(Tdist+d[route[j]][i]+d[i][route[j+1]]-d[route[j]][route[j+1]]<=TIMELIMIT)) {
		indexcust=i;
		indexplace=j+1;
		min=d[route[j]][i]+d[i][route[j+1]]-d[route[j]][route[j+1]];
	      }
	    }
	    else 
	      break;
	  } //for j
      }
    }//for i
    
    //    printf ("indexcust=%d, index place=%d \n",indexcust,indexplace);

    if (indexcust!=-1) { //means a nearest feasible customer node is found 
      p2=indexcust;
      Visited[p2]=1;
      Tdemand=Tdemand+Demand[p2];
      Tdist=Tdist+min;
     
      //Shift the customers in the route by one
      for (i=NCust;i>=indexplace;i--)
	route[i+1]=route[i];
      route[indexplace]=p2;

      /*
      printf("The route changed to ="); 
      for (i=0;i<NCust+2;i++){
	if (route[i]!=-1)
	  printf("%d ",route[i]);
	else 
	  break;
      }
      printf("\n");
      */
    }

    else { //RETURN TO DEPOT

      for (i=1;i<NCust+2;i++) {
	if (route[i]!=-1){
	  RouteSeq[count]=route[i];
	  count++; 
	  route[i]=0;
	}
	else {
	  route[i]=0;
	  break;
	}
      }

      p1=fac+NCust;
      // RouteSeq[count]=p1;
      //count++; 
      numveh++;

      unvisited=0;
      for (i=0;i<NCust;i++){
	if (Visited[i]==0) {
	  unvisited=1;
	  Visited[i]=1;
	  route[0]=NCust+fac;
	  route[1]=i;
	  route[2]=NCust+fac;
	  route[3]=-1;
	  Tdemand=Demand[i];
	  Tdist=d[route[0]][route[1]]+d[route[1]][route[2]];
	  break;
	}
      }
      /*
      printf("New route is ="); 
      for (i=0;i<NCust+2;i++){
	if (route[i]!=-1)
	  printf("%d ",route[i]);
	else 
	  break;
      }
      printf("\n");
      */

    } //else

    //  printf("p1=%d\n",p1);
    
    
  }//while unvisited==1

  if (writelabellist){
    printf("Routes are [ %d ",NCust+fac);
    for (i=0;i<count;i++)
      printf("%d ",RouteSeq[i]);
    //printf("...numveh=%d\n",numveh);
    printf("\n");
  }

  BinList=(double *)calloc (numveh, sizeof(double));
  indexarray=(int *)calloc (numveh, sizeof(int));
  whichbin=(int *)calloc (numveh, sizeof(int));
  BinListCap=(int *)calloc (numveh, sizeof(int));
  BinListSize=(int *)calloc (numveh, sizeof(int));
  BinListSeq=(int **)calloc (numveh, sizeof(int*));

  i=0;
  BinList[i]=d[fac+NCust][RouteSeq[0]];
  BinListCap[i]=Demand[RouteSeq[0]];
  BinListSize[i]=0;
  BinListSeq[i]=(int *)calloc (NCust, sizeof(int));
  BinListSeq[i][BinListSize[i]]=RouteSeq[0];
  BinListSize[i]++;
  
  for (j=0;j<count-1;j++){
    BinList[i]=BinList[i]+d[RouteSeq[j]][RouteSeq[j+1]];
    if (RouteSeq[j+1]<NCust){
      BinListCap[i]=BinListCap[i]+Demand[RouteSeq[j+1]];
      BinListSeq[i][BinListSize[i]]=RouteSeq[j+1];
      BinListSize[i]++;
    }
    if (RouteSeq[j+1]>=NCust){
      i++;
      if (i<numveh) {
	BinListCap[i]=0;
	BinListSize[i]=0;
	BinListSeq[i]=(int *)calloc (NCust, sizeof(int));
	
      }
    }
  }
  
  tvarcost=0;
  
  for (i=0;i<numveh;i++) {
    //printf(" %d ",BinList[i]);
    tvarcost=tvarcost+(double)BinList[i]*VOcost/60;
  }
  //printf("\n");
  
  ColumnList=*ColList;
  n_cols=0;
  n_nz=0;
  
  if (writelabellist){
    for (i=0;i<numveh;i++) {
      if (BinListSize[i]>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=BinListCap[i];
	coltemp->labeldata.Time=BinList[i];
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
	
	for (j=0;j<BinListSize[i];j++){
	  coltemp->labeldata.nodeseq[BinListSeq[i][j]]=j+1;
	}
	
	coltemp->next=ColumnList;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	coltemp->prev=0;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;
	n_nz=n_nz+BinListSize[i];
      } // if (BinListSize[i]>1){
    }
  }
  
  
  for (i=0;i<numveh;i++) 
    indexarray[i]=i;
  
  numbin= BestFitDecreasing (BinList,numveh,TIMELIMIT,&indexarray,&whichbin ) ;
  /*
  printf("index array:\n");
  for (i=0;i<numveh;i++)
    printf(" %d ",indexarray[i]);
  printf("\n");
  */
  /*
  printf("which bin array:\n");
  for (i=0;i<numveh;i++)
    printf(" %d ",whichbin[i]);
  printf("\n");
  */
  if (writelabellist){
    printf("Routes combined: ");
    for (i=0;i<numveh;i++)
      printf(" %d in pairing %d, ",i,whichbin[i]);
    printf("\n");
  }
 
  if (writelabellist){  
    
    x=0;
    
    for(i=0;i<numbin;i++){
      
      ttime=0;
      tcap=0;
      tsize=0;
      add=0;

      for (j=0;j<numveh;j++){
	if (whichbin[j]==i) {
	  ttime=ttime+BinList[j];
	  tcap=tcap+BinListCap[indexarray[j]];
	  tsize=tsize+BinListSize[indexarray[j]];
	  add++;

	}
      }
      if (add>1){
	coltemp=(LABEL_NODE*) calloc(1,sizeof(LABEL_NODE));
	coltemp->labeldata.Cap=tcap;
	coltemp->labeldata.Time=ttime;
	coltemp->labeldata.unreach=fac;
	coltemp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
	
	
	x=x+tsize;
	
	t=1;
	for (j=0;j<numveh;j++){
	  if (whichbin[j]==i) {
	    for (k=0;k<BinListSize[indexarray[j]];k++){
	      coltemp->labeldata.nodeseq[BinListSeq[indexarray[j]][k]]=t;
	      t++;
	    }
	    t++;
	  }
	}	
	
	coltemp->next=ColumnList;
	coltemp->prev=0;
	if (ColumnList)
	  ColumnList->prev=coltemp;
	ColumnList=coltemp;
	coltemp=0;
	n_cols++;
	
      }
    }
    n_nz=n_nz+x;
    *numcust=n_nz;
    *numcols=n_cols;
    *ColList=ColumnList;
  }
  

  ColumnList=0;
  free(RouteSeq);
  RouteSeq=0;
 
  
  free(BinList);
  free(indexarray);
  free(whichbin);
  free(BinListCap);
  free(BinListSize);
  
  for (i=0;i<numveh;i++){  
    free(BinListSeq[i]);
    BinListSeq[i]=0;
  }
  free(BinListSeq);
  BinListSeq=0;
  BinList=0;
  BinListCap=0;
  BinListSize=0;
  indexarray=0;
  whichbin=0;
  
  free(Visited);
  Visited=0;

  free(route);
  route=0;
  tcost=(double)numbin*VFcost+tvarcost;
  
  

  return(tcost);
  

/*
Nearest Insertion Heuristic
Step 1: Choose an arbitrary node i and let the cycle C consist of only i.
Step 2: Find a node outside C closest to a node in C; call it k.
Step 3: Find an edge (i, j) in C such that d(i, k) + d(k, j) is minimal.
Step 4: Construct a new cycle C by replacing (i, j) with (i, k) and (k, j).
Step 5: If the current cycle C contains all vertices, stop. Otherwise, 
go to step 2.
*/

}

/*
 * *********************************************************
 */

int Mod_DROP_HeurForFacilities ( int NFac, int NCust, double **d, int TIMELIMIT, int FCap, double *FCost, double VOcost,int *Demand,int ***customers, int **Selected,int VCap, double VFcost,int heuristic,double *Xcoor, double *Ycoor){
 
  /*
   * Modified DROP heuristic to pick facilities to construct a feasible solution.
   * Calls assignmentOfCust.
   * Function returns -1 if the customers cannot be assigned to facilities.  
   */
  
  //heuristic=1 if CW,
  //heuristic=2 if NN
  //heuristic=3 if SW
  //heuristic=4 if NI

  double *TCost;
  int *feas;
  int *Open;
  int i,j,drop,k,t;
  int **assignment;
  LABEL_NODE *ColList;
  int n_cols,n_nz;
 
  int index;
  double min;

  ColList=0; 
  TCost=(double *)calloc (NFac,sizeof(double));
  //assignment=(int**)calloc (NFac,sizeof(int*));
  assignment=*customers;

  feas=(int *)calloc (NFac,sizeof(int));
  //Open= (int *)calloc (NFac,sizeof(int));
  Open=*Selected;
  //customers=(int **) calloc (NFac,sizeof(int*));
  //countfac=(int*)calloc(NFac,sizeof(int));

  /*
   * INITIALIZATION
   */
  for (i=0;i<NFac;i++){ 
    Open[i]=1;
    assignment[i]=(int*)calloc (NCust+1,sizeof(int));
  }

  i= assignmentOfCust(d,Open, &assignment,FCap,Demand,NFac,NCust,TIMELIMIT);
  if (!i) {
    printf("Customers CANNOT BE assigned to any of the facilities \n");
    return(-1);
  }

  for (j=0;j<NFac;j++){
    if ((Open[j]==1)&&(assignment[j][0]==-1))
      Open[j]=0;
  }
     

  /*
    if (heuristic==4){
  
    for (j=0;j<NFac;j++){
      if (Open[j]==1){
	printf("Facility %d is OPEN: [ ",j);
	for (i=0;i<NCust;i++){
	  if (assignment[j][i]!=-1)
	    printf(" %d ",assignment[j][i]);
	  else 
	    break;
	}
	printf("\n");
      }
    }
  }
  */

  
  drop=1;
  
  while (drop){
    
    for (i=0;i<NFac;i++) {
      if ((Open[i])) {
	
	TCost[i]=0;
	for (j=0;j<NFac;j++) {
	  if ((i!=j)&&(Open[j]))
	    TCost[i]=TCost[i]+FCost[j];
	}
        //drop i and calculate operating cost 
	Open[i]=0;
	feas[i]= assignmentOfCust(d,Open, &assignment,FCap,Demand,NFac,NCust,TIMELIMIT);
	//printf("\n");

	/*
	if(heuristic==4){	
	  printf("CLOSE fac %d \n",i);
	  if (feas[i]){
	    for (t=0;t<NFac;t++){
	      if (Open[t]==1){
		printf("The customers assigned to facility %d are:[ ",t);
		for (k=0;k<NCust;k++){
		  if (assignment[t][k]!=-1){
		    printf(" %d:%5.1lf ",assignment[t][k],2*d[assignment[t][k]][t+NCust]);
		  }
		  else 
		    break;
		}
	      printf("\n");
	      }
	    }
	  }
	}
	*/

	Open[i]=1;

	/*
	//CALCULATE COST USING a heuristic and BIN PACKING
	*/

	if (feas[i]==1) {
	  for (k=0;k<NFac;k++){
	    if ((k!=i)&&(Open[k])){
	      //printf("Facility=%d is closed and Call CW for facility=%d \n",i, k);
	      if(heuristic==1)
		TCost[i]=TCost[i]+clark_wright_Alg (d,NCust,assignment[k], Demand, TIMELIMIT, VCap, k, NCust, VFcost, VOcost,&n_nz, &n_cols,&ColList,0);
	      if(heuristic==2)
		TCost[i]=TCost[i]+NearestNeighbor (d,assignment[k], Demand, TIMELIMIT, VCap, k, NCust, VFcost, VOcost,&n_nz, &n_cols,&ColList,0);
	      if(heuristic==3)
		TCost[i]=TCost[i]+SweepHeuristic(d,Xcoor,Ycoor,assignment[k], Demand, TIMELIMIT, VCap, k, NCust, VFcost, VOcost,&n_nz, &n_cols,&ColList,0);
	      if(heuristic==4)
		TCost[i]=TCost[i]+NearestInsertionHeuristic (d, assignment[k], Demand, TIMELIMIT, VCap, k, NCust, VFcost, VOcost,&n_nz, &n_cols,&ColList,0);
	    }
	  }
	}
	
	else {
	  TCost[i]=INFINITY;
	}

      }//if open and 
    }//for i

    //Choose the facility with min cost

    min=INFINITY;
    index=-1;
    
    /*
    for (i=0;i<NFac;i++) 
      printf("Open[%d]=%d,TCost[%d]=%lf \n",i,Open[i],i,TCost[i]);
    */

    for (i=0;i<NFac;i++) {
      if ((Open[i])&&(TCost[i]<min)) {
	index=i;
	min=TCost[i];
      }
    }
    if (index !=-1)
      Open[index]=0;
    else
      drop=0;

    // printf("index=%d\n",index);

  }
  

  j= assignmentOfCust(d,Open, &assignment,FCap,Demand,NFac,NCust,TIMELIMIT);
  if (!j) {
    printf("Customers CANNOT BE assigned to any of the facilities ERROR!\n");
    return(-1);
  }

  
  for (j=0;j<NFac;j++){
    if (Open[j]){
      printf("The customers assigned to facility %d are:[ ",j);
      for (i=0;i<NCust;i++){
	if (assignment[j][i]!=-1)
	  printf(" %d ",assignment[j][i]);
	else 
	  break;
      }
      printf("\n");
    }
  }
  


  *customers=assignment;
  *Selected=Open; 
  assignment=0;
  Open=0;
  
  
  //free(Open);
  //Open=0;
  free(TCost);
  TCost=0;
  free(feas);
  feas=0;
  
  
  return(0);
}

int generate_1_cust(double **d,int FAC, LABEL_NODE **List) 
{
  LABEL_NODE *SolnList,*temp;
  int i;
  int n_cols;
  double ttime;
  //int ttime;

  SolnList=*List;
  n_cols=0;
  
  for (i=0;i<NCust;i++){
    if (Demand[i]<=VCap){
      ttime=d[i][NCust+FAC]+d[NCust+FAC][i];
      if (ttime<=TIMELIMIT){
	temp=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));

        temp->labeldata.Cap=Demand[i];
        temp->labeldata.Time=ttime;
	temp->labeldata.unreach=FAC;
	temp->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
        temp->labeldata.nodeseq[i]=1;

	temp->prev=0;
	temp->next=SolnList;
	if (SolnList)		 
	  SolnList->prev=temp;
	SolnList=temp;
	temp=0;
	n_cols++;
      }
    }
  } //for

  *List=SolnList;
  SolnList=0;

  return(n_cols);
}

void Assign_Cust_basedon_ratio(double **d,int ***CustAsgn, int nCust,int nFac, int TIMELIMIT,double ratio){

  int **assign,*countfac;
  int i,j;

  countfac=(int *)calloc (nFac,sizeof(int));
  for (i=0;i<nFac;i++){ 
    countfac[i]=0;
  }
  
  assign=*CustAsgn;
  for (i=0;i<nFac;i++){ 
    for (j=0;j<nCust+1;j++) 
      assign[i][j]=0;
  }


  for (j=0;j<nFac;j++) {
    for(i=0;i<nCust;i++){
      if (d[i][nCust+j]+d[j+nCust][i]<=(double)TIMELIMIT*ratio){
	assign[j][countfac[j]]=i;
	countfac[j]++;
      }
    }
  }//for

  for (i=0;i<nFac;i++){
    assign[i][countfac[i]]=-1;
  }

  *CustAsgn=assign;
  assign=0;

  free(countfac);
  countfac=0;
  
}

double Initial_Column_Generator(double **d,double *Xcoor, double *Ycoor, LABEL_NODE **ReturnInitCols,int *numcust, int *numcols, double closeness) {

  int i,j,num_nz,num_col;  
  int **customers, **cust_bo_closeness;
  int *Selected;
  double upperbound,TotalCost,x;
  int *numNZ;
  int *numCOL;
  LABEL_NODE *ListofCols;
  //int count_list;

  ListofCols=*ReturnInitCols;

  /*
   * ****First Do Drop heuristic + C-W heuristic and find Upper bound + columns 
   */
  
  Selected=(int*)calloc(NFac,sizeof(int));
  customers=(int **) calloc (NFac,sizeof(int*));
  numNZ=(int*)calloc(NFac,sizeof(int));
  numCOL=(int*)calloc(NFac,sizeof(int));
  cust_bo_closeness=(int **) calloc (NFac,sizeof(int*));
 
  for (i=0;i<NFac;i++)
    cust_bo_closeness[i]=(int*)calloc (NCust+1,sizeof(int));


  Assign_Cust_basedon_ratio(d,&cust_bo_closeness, NCust,NFac, TIMELIMIT,closeness);
  /*
  for (j=0;j<NFac;j++){
    printf("The customers assigned to facility %d are:[ ",j);
    for (i=0;i<NCust;i++){
      if (cust_bo_closeness[j][i]!=-1)
	printf(" %d ",cust_bo_closeness[j][i]);
      else 
	break;
    }
    printf("\n");
    
  }
  */

  j=Mod_DROP_HeurForFacilities (NFac, NCust, d, TIMELIMIT, FCap, FCost, VOcost,Demand,&customers,&Selected,VCap, VFcost,1,0,0);
  
  if (j<0) {
    printf("ERROR:Cannot find a feasible assignment of customers to facilities! \n");
    exit(9);
  }

  for (j=0;j<NFac;j++) {
    numNZ[j]=0;
    numCOL[j]=0;
  }

  //******************************************************************************************************************** 
  //CALL CLARK-WRIGHT ALGORITHM 
  
  upperbound=0;
 
  for (j=0;j<NFac;j++) {
    if (Selected[j]){
      num_col=0;
      num_nz=0;
      upperbound= upperbound+clark_wright_Alg (d,NCust,customers[j], Demand, TIMELIMIT, VCap, j ,NCust,VFcost, VOcost,&num_nz, &num_col, &ListofCols,1)+FCost[j];
      // printf("CW Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
      numNZ[j]=numNZ[j]+num_nz;
      numCOL[j]=numCOL[j]+num_col;
    }
    else {
      if (cust_bo_closeness[j][0]!=-1) {
	num_col=0;
	num_nz=0;
	x=clark_wright_Alg (d,NCust,cust_bo_closeness[j], Demand, TIMELIMIT, VCap, j ,NCust,VFcost, VOcost,&num_nz, &num_col, &ListofCols,1);
	//	printf("CW Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
	numNZ[j]=numNZ[j]+num_nz;
	numCOL[j]=numCOL[j]+num_col;
      }
    }
  }
  
  for (j=0;j<NFac;j++){
    if (customers[j])
      free (customers[j]);
    customers[j]=0;
  }
  
  printf("Total cost of  CLARK-WRIGHT ALGORITHM is %lf \n",upperbound);
  /*
  printf("The ListofCols: \n");
  count_list=0;
  temp=ListofCols;
  while(temp) {
    count_list++;
    printf("fac=%d, route= ",temp->labeldata.unreach);
    for (j=0; j < NCust; j++) {
      if (temp->labeldata.nodeseq[j]> 0) { 
       printf("%d:%d ",j,temp->labeldata.nodeseq[j]);
     } // if 
   } // for j
    printf("\n");
    temp=temp->next;
  }
    
  printf("# of columns in the list=%d \n",count_list);
  */

  //******************************************************************************************************************** 
  //NEAREST NEIGBOUR HEURISTIC
  
  for (j=0;j<NFac;j++) {
    Selected[j]=0;
    customers[j]=0;
  }

 
  
  j=Mod_DROP_HeurForFacilities (NFac, NCust, d, TIMELIMIT, FCap, FCost, VOcost,Demand,&customers,&Selected,VCap, VFcost,2,0,0);
  
 
  TotalCost=0;
  for (j=0;j<NFac;j++) {
    if (Selected[j]){
      num_col=0;
      num_nz=0;
      TotalCost=TotalCost+NearestNeighbor (d,customers[j], Demand, TIMELIMIT, VCap, j, NCust, VFcost, VOcost,&num_nz, &num_col, &ListofCols,1)+FCost[j];
      // printf("NN Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
      numNZ[j]=numNZ[j]+num_nz;
      numCOL[j]=numCOL[j]+num_col;
    } //if
    
    else {
      if (cust_bo_closeness[j][0]!=-1) {
	num_col=0;
	num_nz=0;
	x=NearestNeighbor (d,cust_bo_closeness[j], Demand, TIMELIMIT, VCap, j, NCust, VFcost, VOcost,&num_nz, &num_col, &ListofCols,1);
	//	printf("NN Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
	numNZ[j]=numNZ[j]+num_nz;
	numCOL[j]=numCOL[j]+num_col;
      }
    } //else
    
  }
  
  for (j=0;j<NFac;j++){
    if (customers[j])
      free (customers[j]);
    customers[j]=0;
  }
  
  printf("Total cost of NEAREST NEIGBOUR HEURISTIC is %lf \n",TotalCost);
  if ((TotalCost>0)&&(TotalCost<upperbound))
    upperbound=TotalCost;

  /*
  printf("The ListofCols: \n");

  count_list=0;
  temp=ListofCols;
  while(temp) {
    count_list++;
    printf("fac=%d, route= ",temp->labeldata.unreach);
    for (j=0; j < NCust; j++) {
      if (temp->labeldata.nodeseq[j]> 0) { 
       printf("%d:%d ",j,temp->labeldata.nodeseq[j]);
     } // if 
   } // for j
    printf("\n");
    temp=temp->next;
  }
    
  printf("# of columns in the list=%d \n",count_list);
  */

  //******************************************************************************************************************** 

  //CALL SWEEP HEURISTC
  
  for (j=0;j<NFac;j++) {
    Selected[j]=0;
    customers[j]=0;
  }
  
  
  j=Mod_DROP_HeurForFacilities (NFac, NCust, d, TIMELIMIT, FCap, FCost, VOcost,Demand,&customers,&Selected,VCap, VFcost,3,Xcoor,Ycoor);

 //  printf("\n\n");
  /*
  for (j=0;j<NFac;j++) {
    if (Selected[j]){
      printf("Facility %d is selected. Customers assigned are [ \n",j);
      for (i=0;i<NCust;i++) {
	if (customers[j][i]!=-1)
	  printf(" %d ",customers[j][i]);
	else 
	  break;
      }
      printf("]\n");
    }
  }
  */

  if (j<0) {
    printf("ERROR:Cannot find a feasible assignment of customers to facilities! \n");
    exit(9);
  }


  TotalCost=0;
  for (j=0;j<NFac;j++) {
    if (Selected[j]){
      num_col=0;
      num_nz=0;
      TotalCost=TotalCost+SweepHeuristic(d,Xcoor,Ycoor,customers[j], Demand, TIMELIMIT, VCap, j, NCust, VFcost, VOcost, &num_nz, &num_col, &ListofCols,1)+FCost[j];
      // printf("SWP Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
      numNZ[j]=numNZ[j]+num_nz;
      numCOL[j]=numCOL[j]+num_col;
    }
    else {
      if (cust_bo_closeness[j][0]!=-1) {
	num_col=0;
	num_nz=0;
	x=NearestNeighbor (d,cust_bo_closeness[j], Demand, TIMELIMIT, VCap, j, NCust, VFcost, VOcost,&num_nz, &num_col, &ListofCols,1);
	//	printf("SWP Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
	numNZ[j]=numNZ[j]+num_nz;
	numCOL[j]=numCOL[j]+num_col;
      }
    } //else
  }
  
  printf("Total cost of SWEEP HEURISTC is %lf \n",TotalCost);
  
  if ((TotalCost>0)&&(TotalCost<upperbound))
    upperbound=TotalCost;

  for (j=0;j<NFac;j++){
    if (customers[j])
      free (customers[j]);
    customers[j]=0;
  }
  


  /* 
   printf("The ListofCols: \n");

   count_list=0;
  temp=ListofCols;
  while(temp) {
    count_list++;
    printf("fac=%d, route= ",temp->labeldata.unreach);
    for (j=0; j < NCust; j++) {
      if (temp->labeldata.nodeseq[j]> 0) { 
       printf("%d:%d ",j,temp->labeldata.nodeseq[j]);
     } // if 
   } // for j
    printf("\n");
    temp=temp->next;
  }
    
  printf("# of columns in the list=%d \n",count_list);
  */
  //******************************************************************************************************************** 

  //CALL NEAREST INSERTION  HEURISTC
  
  for (j=0;j<NFac;j++) {
    Selected[j]=0;
    customers[j]=0;
  }
  
  
  j=Mod_DROP_HeurForFacilities (NFac, NCust, d, TIMELIMIT, FCap, FCost, VOcost,Demand,&customers,&Selected,VCap, VFcost,4,0,0);

 //  printf("\n\n");
  printf("Drop heur is done. \n");
  for (j=0;j<NFac;j++) {
    if (Selected[j]){
      printf("Facility %d is selected. Customers assigned are [ \n",j);
      for (i=0;i<NCust;i++) {
	if (customers[j][i]!=-1)
	  printf(" %d ",customers[j][i]);
	else 
	  break;
      }
      printf("]\n");
    }
  }
 
  if (j<0) {
    printf("ERROR:Cannot find a feasible assignment of customers to facilities! \n");
    exit(9);
  }

  TotalCost=0;
  for (j=0;j<NFac;j++) {
    if (Selected[j]){
      num_col=0;
      num_nz=0;
      TotalCost=TotalCost+NearestInsertionHeuristic (d, customers[j], Demand, TIMELIMIT, VCap, j, NCust, VFcost, VOcost,&num_nz, &num_col, &ListofCols,1)+FCost[j];
      //printf("NI Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
      numNZ[j]=numNZ[j]+num_nz;
      numCOL[j]=numCOL[j]+num_col;
    }
    else {
      if (cust_bo_closeness[j][0]!=-1) {
	num_col=0;
	num_nz=0;
	x=NearestNeighbor (d,cust_bo_closeness[j], Demand, TIMELIMIT, VCap, j, NCust, VFcost, VOcost,&num_nz, &num_col, &ListofCols,1);
	//printf("NI Alg: fac= %d, numcols= %d, numNZ=%d \n",j,num_col,num_nz);
	numNZ[j]=numNZ[j]+num_nz;
	numCOL[j]=numCOL[j]+num_col;
      }
    } //else
  }
  
  printf("Total cost of  NEAREST INSERTION  HEURISTC is %lf \n",TotalCost);
  if ((TotalCost>0)&&(TotalCost<upperbound))
    upperbound=TotalCost;

  /*
  printf("The ListofCols: \n");
  
  count_list=0;
  temp=ListofCols;
  while(temp) {
    count_list++;
    printf("fac=%d, route= ",temp->labeldata.unreach);
    for (j=0; j < NCust; j++) {
      if (temp->labeldata.nodeseq[j]> 0) { 
       printf("%d:%d ",j,temp->labeldata.nodeseq[j]);
     } // if 
   } // for j
    printf("\n");
    temp=temp->next;
  }
    
  printf("# of columns in the list=%d \n",count_list);
  */

  //******************************************************************************************************************** 

  for (j=0;j<NFac;j++) {
    i=generate_1_cust(d,j, &ListofCols);
    numNZ[j]=numNZ[j]+i;
    numCOL[j]=numCOL[j]+i;
  }

  num_nz=0;
  num_col=0;
  for (j=0;j<NFac;j++) {
    num_nz=num_nz+numNZ[j];
    num_col=num_col+numCOL[j];
  }
  /*
  printf("The lsit of initial columns: \n");
  count_list=0;
  temp=ListofCols;
  while(temp) {
    count_list++;
    printf("fac=%d, route= ",temp->labeldata.unreach);
    for (j=0; j < NCust; j++) {
      if (temp->labeldata.nodeseq[j]> 0) { 
       printf("%d:%d ",j,temp->labeldata.nodeseq[j]);
     } // if 
   } // for j
    printf("\n");
    temp=temp->next;
  }
    
  printf("# of columns in the list=%d \n",count_list);
  */

  *numcust=num_nz;
  *numcols=num_col;
  
  if (Selected)
    free(Selected);
  Selected=0; 
   
  for (j=0;j<NFac;j++) {
    if (customers[j]) {
      free(customers[j]);
      customers[j]=0;
    }
    if (cust_bo_closeness[j]){
      free(cust_bo_closeness[j]);
      cust_bo_closeness[j]=0;
    }
  }
  if (customers)
    free(customers);
  customers=0;
  if (cust_bo_closeness)
    free(cust_bo_closeness);
  cust_bo_closeness=0;

  if (numNZ)
    free(numNZ);

  if (numCOL)
    free(numCOL);

  numNZ=0;
  numCOL=0;

  *ReturnInitCols= ListofCols;
  ListofCols=0;




  return(upperbound);

}
