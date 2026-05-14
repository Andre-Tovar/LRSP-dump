//April 19 2006
//finds facility locations using DROP heuristic and calls c-w and bin packing
// assign customers, do clark and wright, and bin packing to find vehicle routes.

#include "header_cw.h"

int BestFitDecreasing (int *BinList, int numveh, int TIMELIMIT) {

  int i,j,temp,min,index,tnumbin;
  int *binsize;
  int *whichbin;

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
      }
    }
  }

  /*
  printf("The sorted binlist \n");
  for (i=0;i<numveh;i++)
    printf(" %d ",BinList[i]);
  printf("\n");
  
  printf("TIMELIMIT=%d \n",TIMELIMIT);
  */
  //i=INFINITY;
  //printf("infinity=%d \n",i);

  binsize=(int*)calloc (numveh,sizeof(int));
  whichbin=(int*)calloc (numveh,sizeof(int));

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

  if(binsize)
    free(binsize);
    
  
  if (whichbin)
    free(whichbin); 

  return(tnumbin);


}




double clark_wright_Alg (double **d,int ncust, int *customers, int *Demand, int TIMELIMIT, int VCap, int fac,int NCust, double VFcost, double VOcost) {
  //, ROUTE **List, int *totalnumroute
  SAVINGLIST *List, *temp;
  int i,j,node1,node2;
  ROUTE *RouteList, *route,*route1,*route2, *route_n1,*route_n2;
  int *unassigned;
  int found_n1, found_n2,nold,nnew,time;
  double tvarcost,tcost;
  int numveh;
  int *BinList, numbin;

  // VCap=400;
  //TIMELIMIT=400;

  if ((ncust<2) || (customers[1]==-1)){
    printf("There is only one customer for facility %d, NO Clark-Wright\n",fac);
    tcost=VFcost+2*d[fac+NCust][customers[0]]*VOcost/60;
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

  
  // printf("VCap=%d, Timelimit=%d \n",VCap, TIMELIMIT);
  
  temp=List;
  while (temp) {
    node1=temp->n1;
    node2=temp->n2;
    /*
    route=RouteList;
    printf("ROUTES in the List\n");
    while (route) {
      printf("cap=%d, time=%d, [",route->cap, route->time);
      for (i=0;i<route->size;i++)
	printf(" %d ",route->seq[i]);
      printf("]\n");
      route=route->next;
    }
    route=0;
    printf("Pick node1=%d, demand=%d -- node2=%d demand=%d\n",customers[node1],Demand[customers[node1]],customers[node2],Demand[customers[node2]]);
    */
    
    
    if ((unassigned[node1]==1) && (unassigned[node2]==1)) { //None of them are assigned 
      time=d[NCust+fac][customers[node1]]+d[customers[node1]][customers[node2]]+d[NCust+fac][customers[node2]];
      if (time<=TIMELIMIT) {
	//printf("Both are unassigned, form a route\n");
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
      //printf("Node %d is unassigned, node %d is assigned\n",customers[nnew],customers[nold]);
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
      // printf("Node %d is assigned, node %d is assigned\n",customers[node1],customers[node2]);
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

  while (route) {
    numveh++;
    tvarcost=route->time*VOcost/60+tvarcost;
    printf("route=\n");
    for (i=0;i<route->size;i++) 
      printf(" %d ",route->seq[i]);
    printf("\n");
    route=route->next;
  }

  printf("unassigned customers:[");
  for (i=0;i<ncust;i++){
    if (customers[i]!=-1) {
      if (unassigned[i]) {
	printf(" %d ",customers[i]);
	numveh++;
	tvarcost=tvarcost+ 2*d[fac+NCust][customers[i]]*VOcost/60;
	/*
	route1=(ROUTE *) calloc (1,sizeof(ROUTE));
	route1->cap=Demand[customers[i]];
	route1->time=2*d[fac+NCust][customers[i]];
	route1->size= 1;
	route1->left=customers[i];
	route1->right=customers[i];
	route1->seq=(int*)calloc(route1->size,sizeof(int));
	route1->seq[0]=customers[i];
	route1->next=0;
	route1->prev=0;
	route1->next= RouteList;
	if (RouteList)
	  RouteList->prev=route1;
	RouteList=route1;
	*/
      }
    }
    else 
      break;
  }
  printf("]\n");
  printf("number of paths is %d \n",numveh);
  

  //printf("total variable cost for fac %d is %lf \n",fac,tvarcost);

  BinList=(int *)calloc (numveh, sizeof(int));
  route=RouteList;
  i=0;
  while (route) {
    BinList[i]=route->time;
    route=route->next;
    i++;
  }
  for (j=0;j<ncust;j++){
    if (customers[j]!=-1) {
      if (unassigned[j]) {
	BinList[i]=2*d[fac+NCust][customers[j]];
	i++;
      }
    }
    else
      break;
  }

  numbin= BestFitDecreasing (BinList,numveh,TIMELIMIT) ;

  tcost=numbin*VFcost+tvarcost;
  if(BinList)
    free(BinList);
  BinList=0;

  route=RouteList;
  while (route){
    RouteList=route->next;
    if (RouteList)
      RouteList->prev=0;
    route->prev=0;
    route->next=0;
    free(route->seq);
    free(route);
    route=0;
    route=RouteList;
  }
  if (RouteList)
    free(RouteList);
  RouteList=0;

  //printf("total variable cost + bin cost for fac %d is %lf \n",fac,tcost);
  return(tcost);   
}


int assignmentOfCust(double **d,int *Open, int ***CustAsgn, int FCap, int *Demand, int NFac,int NCust,int TIMELIMIT){
  /*
   * Assigns customers to open facilities. Choses closest facility. But also considers timelimit and facility capacity
   * If the assignment of customers to open facilities is infeasible, the function returns 0. Otherwise returns 1.
   */
  int **assign,j;
  int *TCap;
  int min,i, index;
  int *countfac,feas;

  feas=1;

  /*
  printf("FCap=%d,NFac=%d,NCust=%d \n",FCap,NFac,NCust);
  for (i=0;i<NFac;i++)
    printf("Open[%d]=%d \n",i,Open[i]);
  */


  assign=*CustAsgn;
  for (i=0;i<NFac;i++){ 
    for (j=0;j<NCust;j++) 
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
      if(countfac[i]!=0)
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

  if (TCap)
    free(TCap);
  TCap=0;
  if(countfac)
    free(countfac);
  countfac=0;
  
  *CustAsgn=assign;
  assign=0;
  
  return(feas);

}



int Mod_DROP_HeurForFacilities (int num_REQ, int NFac, int NCust, double **d, int TIMELIMIT, int FCap, double *FCost, double VOcost,int *Demand,int ***customers, int **Selected,int VCap, double VFcost){
  /*
   * Modified DROP heuristic to pick facilities to construct a feasible solution.
   * Calls assignmentOfCust.
   * Function returns -1 if the customers cannot be assigned to facilities.  
   */

  double *TCost;
  int *feas;
  int *Open;
  int i,j,drop,k;
  int **assignment;

 
  int index;
  double min;

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
    assignment[i]=(int*)calloc (NCust,sizeof(int));
  }

  i= assignmentOfCust(d,Open, &assignment,FCap,Demand,NFac,NCust,TIMELIMIT);
  if (!i) {
    printf("Customers CANNOT BE assigned to any of the facilities \n");
    return(-1);
  }

  /*
  for (j=0;j<NFac;j++){
    if (Open[j]==1){
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
	if (feas[i]){
	  for (t=0;t<NFac;t++){
	    if (Open[t]==1){
	      printf("The customers assigned to facility %d are:[ ",t);
	      for (k=0;k<NCust;k++){
		if (assignment[t][k]!=-1)
		  printf(" %d ",assignment[t][k]);
		else 
		  break;
	      }
	      printf("\n");
	    }
	  }
	}
	*/
	Open[i]=1;
	//	printf("totalfixed cost[0]=%lf \n",TCost[0]); 
	if (feas[i]==1) {
	  for (k=0;k<NFac;k++){
	    if ((k!=i)&&(Open[k])){
	      //printf("Facility=%d is closed and Call CW for facility=%d \n",i, k);
	      
	      TCost[i]=TCost[i]+clark_wright_Alg (d,NCust,assignment[k], Demand, TIMELIMIT, VCap, k, NCust, VFcost, VOcost);
	    }
	  }
	}


	else {
	  TCost[i]=INFINITY;
	}

	/*
	if (i==0) {
	  printf("TCost=%lf \n", TCost[i]);
	  exit(1);

	  }*/

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

  /*
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
  */

  *customers=assignment;
  *Selected=Open; 
  assignment=0;
  Open=0;
  
  
  //free(Open);
  //Open=0;
  if(TCost)
    free(TCost);
  TCost=0;

  if (feas)
    free(feas);
  feas=0;
  
  
  return(0);
}


double heurmain2(double **d) {


  int i,j;
  
  int **customers;
   int *Selected;
  double TotalCost;
  // ROUTE *List;
  //int  totalnumroute;

 

  Selected=(int*)calloc(NFac,sizeof(int));
  customers=(int **) calloc (NFac,sizeof(int*));


  j=Mod_DROP_HeurForFacilities (num_REQ, NFac, NCust, d, TIMELIMIT, FCap, FCost, VOcost,Demand,&customers,&Selected,VCap, VFcost);
  
   printf("\n\n");
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
  
  //CALL CLARK-WRIGHT ALGORITHM 
  TotalCost=0;
  for (j=0;j<NFac;j++) {
    if (Selected[j]){

      TotalCost= TotalCost+clark_wright_Alg (d,NCust,customers[j], Demand, TIMELIMIT, VCap, j ,NCust,VFcost, VOcost)+FCost[j];

    }
  }
  printf("Total cost of location routing problem is %lf \n",TotalCost);


  for (j=0;j<NFac;j++) {
    if (customers[j])
      free(customers[j]);
  }
  if (customers)
    free(customers);

  if (Selected)
    free(Selected);
  
  Selected=0;
  customers=0;

  return(TotalCost);
}
