//Aug 8 2006

//Will find columns with negative reduced cost 
//using clarke and Wright and possibly bin packing.

#include "header_cw.h"

int KnapsackSolnDynProg (double *v, int *w, int n, int W,double vdual,double FixedCost,int *X) {

  int i,k,t;
  double c[n+1][W+1];
  
  int new_col;
  
  for (k = 0;k<=W;k++)
    c[0][k] = 0;
  for (i =1;i<=n;i++) {
    c[i][0] = 0;
    for (k = 1;k<=W;k++) {
      if (w[i] <=k) {
	if (v[i] + c[i-1][k-w[i]] > c[i-1][k])
	  c[i][k] = v[i] + c[i-1][k-w[i]];
	else 
	  c[i][k] = c[i-1][k];
      }
      else c[i][k] = c[i-1][k];
    }
  }

  if (-c[n][W]+FixedCost-vdual<=-PRECISIONLIM){
    t=W;
    for (i=n;i>0;i--){
      if (c[i][t]==c[i-1][t]){
	X[i]=0;
      }
      else {
	X[i]=1;
	t=t-w[i];
      }
    }
  
    t=0;
    for (i=1;i<n+1;i++){
      if (X[i]>0)
	t++;
      printf("X[%d]=%d \n",i,X[i]);
    }

    if (t>1)
      new_col=1;
    else
      new_col=0;
  }
  else 
    new_col=0;
  return(new_col);

}

//It should be an ASYMMETRIC C-W algorithm...

void CWAlgorithm (int **d,double **rc, int *customers, int *Demand, int TIMELIMIT, int VCap, int fac,int NCust, double VFcost, double VOcost, LABEL_NODE **ColumnLabels, double vehicle_dual) {

  //, ROUTE **List, int *totalnumroute
  SAVINGLIST *List, *temp;
  int i,j,node1,node2;
  ROUTE *RouteList, *route,*route1,*route2, *route_n1,*route_n2;
  int *unassigned;
  int found_n1, found_n2,nold,nnew,time;
  int numveh;
  int x;
  double *value;
  int *weight;
  LABEL_NODE *label_temp, *label_list;
  int *knapsackSoln;
  int add_new_col;
  int t,k;

  label_list=*ColumnLabels;


  List=0;
  //the cost between i-->j is not equal to j-->i therefore we have to keep i-->j and j-->i

  for (i=0;i<NCust;i++) {
    if (customers[i]!=-1) {
      for (j=i+1;j<NCust;j++){
	if ((customers[j]!=-1)) {
	  if (Demand[i]+Demand[j]<=VCap){
	    if (d[i][j]<=TIMELIMIT) {
	      temp=(SAVINGLIST *) calloc(1, sizeof(SAVINGLIST));
	      temp->n1=i; // customers
	      temp->n2=j;//customers
	      temp->saving=rc[i][NCust+1]+rc[NCust+1][j]-rc[i][j];
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
	    } //<timelimit
	    if (d[j][i]<=TIMELIMIT) {
	      temp=(SAVINGLIST *) calloc(1, sizeof(SAVINGLIST));
	      temp->n1=j; // customers
	      temp->n2=i;//customers
	      temp->saving=rc[NCust+1][i]+rc[j][NCust+1]-rc[j][i];
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
	    }//<Timelimit
	  }
	}
      } //for
    } //if 
  } //for

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
  */
  
  unassigned=(int*)calloc (NCust,sizeof(int));
  for (i=0;i<NCust;i++)
    unassigned[i]=1;
  
  RouteList=0;

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
    */
    // route=0;
    //printf("Pick node1=%d, demand=%d -- node2=%d demand=%d\n",node1,Demand[node1],node2,Demand[node2]);
    
    
    if ((unassigned[node1]==1) && (unassigned[node2]==1)) { //None of them are assigned 
      time=d[NCust+1][node1]+d[node1][node2]+d[NCust+1][node2];
      if (time<=TIMELIMIT) {
	//printf("Both are unassigned, form a route\n");
	//construct a new route
	route=(ROUTE *) calloc (1,sizeof(ROUTE));
	route->left=node1;
	route->right=node2;
	route->size=2;
	route->cap=Demand[node1]+Demand[node2];
	route->time=d[NCust+1][node1]+d[node1][node2]+d[NCust+1][node2];
	route->next=0;
	route->prev=0;
	route->seq=(int*)calloc(route->size,sizeof(int));
	route->seq[0]=node1;
	route->seq[1]=node2;
	route->cost=rc[NCust+1][node1]+rc[node1][node2]+rc[node2][NCust+1];
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

      route=RouteList;
      while (route) {
	if ((route->left==nold)||(route->right==nold)){
	  if (route->cap + Demand[nnew] <=VCap){
	    if (route->right==node1) { //case a:[...n1]
	      time=route->time-d[node1][NCust+1]+d[node1][nnew]+d[nnew][NCust+1];
	      if (time<=TIMELIMIT){
		//add nnew to the existing route
		unassigned[nnew]=0;
		route1=(ROUTE *) calloc (1,sizeof(ROUTE));
		route1->cap=route->cap + Demand[nnew];
		route1->time=time;
		route1->size= route->size+1;
		
		route1->right=nnew;
		route1->left=route->left;
		route1->seq=(int*)calloc(route1->size,sizeof(int));
		route1->seq[route1->size-1]=nnew;
		for(i=0;i<route1->size-1;i++)
		  route1->seq[i]=route->seq[i];
		route1->cost=route->cost-rc[nold][NCust+1]+rc[nold][nnew]+rc[nnew][NCust+1];
	      
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

	      } //if  <=TIMELIMIT
	      break;

	    }//route->right==node1

	    /*
	    if (route->left==node1) { //case b:[n1..]  -- Take reverse
	      
	      time=d[node2][NCust+1]+d[node1][node2];
	      for(j=route->size-1;j>=1;j--) {
		time=time+d[route->seq[j]][route->seq[j-1]];
	      } 
	      time=time+d[NCust+1][route->seq[route->size-1]];
	      
	      if (time<=TIMELIMIT){
		//add nnew to the existing route
		unassigned[nnew]=0;
		route1=(ROUTE *) calloc (1,sizeof(ROUTE));
		route1->cap=route->cap + Demand[nnew];
		route1->time=time;
		route1->size= route->size+1;
		
		route1->right=nnew;
		route1->left=route->right;
		route1->seq=(int*)calloc(route1->size,sizeof(int));
		route1->seq[route1->size-1]=nnew;
		for(i=0;i<route1->size-1;i++)
		  route1->seq[i]=route->seq[route->size-1-i];

		route1->cost=rc[node2][NCust+1]+rc[node1][node2];
		for(j=route->size-1;j>=1;j--) {
		  route1->cost =route1->cost+rc[route->seq[j]][route->seq[j-1]];
		} 
		route1->cost=route1->cost+rc[NCust+1][route->seq[route->size-1]];
	      } //if  <=TIMELIMIT
	    }//route->left==node1
	    */


	    if (route->left==node2) { //case c:[n2..]
	      time=route->time-d[NCust+1][node2]+d[nnew][node2]+d[NCust+1][nnew];
	      if (time<=TIMELIMIT){
		//add nnew to the existing route
		unassigned[nnew]=0;
		route1=(ROUTE *) calloc (1,sizeof(ROUTE));
		route1->cap=route->cap + Demand[nnew];
		route1->time=time;
		route1->size= route->size+1;
		
		route1->right=route->left;
		route1->left=nnew;
		route1->seq=(int*)calloc(route1->size,sizeof(int));
		route1->seq[0]=nnew;
		for(i=1;i<route1->size;i++)
		  route1->seq[i]=route->seq[i-1];

		route1->cost=route->cost-rc[NCust+1][nold]+rc[nnew][nold]+rc[NCust+1][nnew];
	
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
		
	      } //if  <=TIMELIMIT
	      break;

	    }//route->left==node2
	    
	    /*
	    if (route->right==node2) { //case b:[..n2]
	      
	      time=d[NCust+1][node1]+d[node1][node2];
	      for(j=route->size-1;j>=1;j--) {
		time=time+d[route->seq[j]][route->seq[j-1]];
	      } 
	      time=time+d[route->seq[0]][NCust+1];
	      
	      if (time<=TIMELIMIT){
		//add nnew to the existing route
		unassigned[nnew]=0;
		route1=(ROUTE *) calloc (1,sizeof(ROUTE));
		route1->cap=route->cap + Demand[nnew];
		route1->time=time;
		route1->size= route->size+1;
		
		route1->left=nnew;
		route1->right=route->left;
		route1->seq=(int*)calloc(route1->size,sizeof(int));
		route1->seq[0]=nnew;
		for(i=1;i<route1->size;i++)
		  route1->seq[i]=route->seq[route->size-i];

		route1->cost=rc[NCust+1][node1]+rc[node1][node2];
		for(j=1;j<route1->size-1;j++) {
		  route1->cost =route1->cost+rc[route1->seq[j]][route1->seq[j+1]];
		} 
		route1->cost=route1->cost+rc[route1->seq[route->size-1]][NCust+1];
	      } //if  <=TIMELIMIT
	    }//route->right==node2
	    */


	   
	  }//if Cap is feasible
	  break;
	} //if ((route->left==nold)||(route->right==nold)){
	
	else 
	  route=route->next;
      } //while route
      
    } //else if ..(unassigned[node1]+unassigned[node2]==1) { //one of them is assigned
    
    
      //printf("Node %d is unassigned, node %d is assigned\n",customers[nnew],customers[nold]);
    
    else if (unassigned[node1]+unassigned[node2]==0) { //both are assigned

      // printf("Node %d is assigned, node %d is assigned\n",customers[node1],customers[node2]);
      route=RouteList;
      found_n1=0;
      found_n2=0;
      route_n1=0;
      route_n2=0;
      
      while (route) {
	if ((route->right==node1)) {
	  route_n1=route;
	  found_n1=1;
	}
	if ((route->left==node2)) {
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
	/*
	if ((route_n1->left==node1) && (route_n2->right==node2)){
	  time=route_n1->time+route_n2->time+d[node2][node1]-d[NCust+1][node1]-d[node2][NCust+1];
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

	    route2->cost=route_n1->cost+route_n2->cost+rc[node2][node1]-rc[NCust+1][node1]-rc[node2][NCust+1];

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
	*/

	/*
	 * CASE2:[...n1]  [n2...]
	 */
	
	//else if ((route_n1->right==node1) && (route_n2->left==node2)){
	
	time=route_n1->time+route_n2->time+d[node1][node2]-d[NCust+1][node2]-d[node1][NCust+1];
	
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
	  
	  route2->cost=route_n1->cost+route_n2->cost+rc[node1][node2]-rc[NCust+1][node2]-rc[node1][NCust+1];
	  
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
	//} //else if 

	/*
	 * CASE3:[...n1]  [...n2]
	 */
	/*
	else if ((route_n1->right==node1) && (route_n2->right==node2)){
	
	  // time=route_n1->time+route_n2->time+d[node1][node2]-d[node2][NCust+1]-d[node1][NCust+1];
	  time=route_n1->time+d[node1][node2]-d[node1][NCust+1];
	  for(j=route_n2->size-1;j>=1;j--) {
	    time=time+d[route_n2->seq[j]][route_n2->seq[j-1]];
	  } 
	  time=time+d[route_n2->seq[0]][NCust+1];
	  
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
	    
	    route2->cost=route_n1->cost-rc[node1][NCust+1];
	    
	    for(j=0;j<route_n2->size;j++) {
	      route2->seq[i+j]=route_n2->seq[route_n2->size-1-j];
	      route2->cost=route2->cost+rc[route2->seq[i+j-1]][route2->seq[i+j]];
	    } 
	    
	    route2->cost=route2->cost+rc[route2->seq[route2->size-1]][NCust+1];
	    
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
	*/

	/*
	 * CASE4:[n1...]  [n2...]
	 */
	/*
	else if ((route_n1->left==node1) && (route_n2->left==node2)){
	  
	  
	  time=route_n2->time+d[node1][node2]-d[NCust+1][node2];
	  for(j=route_n1->size-1;j>=1;j--) {
	    time=time+d[route_n1->seq[j]][route_n1->seq[j-1]];
	  } 
	  time=time+d[NCust+1][route_n1->seq[route_n1->size-1]];
	     
	  //time=route_n1->time+route_n2->time+d[node1][node2]-d[NCust+1][node2]-d[NCust+1][node1];
	  
	  if ((route_n1->cap+route_n2->cap<=VCap)&&(time<=TIMELIMIT)) {
	    //combine route and route 1
	    route2=(ROUTE *) calloc (1,sizeof(ROUTE));
	    route2->cap=route_n1->cap + route_n2->cap;
	    route2->time=time;
	    route2->size= route_n1->size+route_n2->size;
	    route2->right=route_n2->right;
	    route2->left=route_n1->right;
	    route2->seq=(int*)calloc(route2->size,sizeof(int));
	    
	    route2->cost=route_n2->cost-rc[NCust+1][node2];
	    x=NCust+1;
	    
	    for(i=0;i<route_n1->size;i++) {
	      route2->seq[i]=route_n1->seq[route_n1->size-1-i];
	      route2->cost=route2->cost+rc[x][route2->seq[i]];
	      x=route2->seq[i];
	    }	   
	    route2->cost=route2->cost+rc[x][node2];
	    
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

	*/
      } //if route_n1, route_n2 are found
    }//if both are assigned
    
    temp=temp->next;
  } //while temp
  
  
  
  route=RouteList;
  //tvarcost=0;
  numveh=0;
  
  while (route) {
    
    numveh++;
    if (route->cost+VFcost-vehicle_dual<-PRECISIONLIM){ //add as a column
      printf("The route is added as a column \n");
      label_temp=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
      label_temp->labeldata.Time=route->time; 
      label_temp->labeldata.Cap=route->cap;
      label_temp->labeldata.nodeseq=(int *) calloc (NCust, sizeof(int));
      label_temp->cond='C';
      
      printf("Route red. cost=%lf, and route=\n",route->cost);
      for (i=0;i<route->size;i++) {
	printf(" %d ",route->seq[i]);
	label_temp->labeldata.nodeseq[route->seq[i]]=i+1;
      }
      printf("\n");
      label_temp->prev=0; 
      label_temp->next=label_list;
      if (label_list)		 
	label_list->prev=label_temp;
      label_list=label_temp;
      label_temp=0;
      
      
      
    }
    else {
      printf("Route red. cost=%lf, and route=\n",route->cost);
      for (i=0;i<route->size;i++) {
	printf(" %d ",route->seq[i]);
      }
      printf("\n");
    }
    
    route=route->next;

  }
  
  
  printf("unassigned customers:[");
  for (i=0;i<NCust;i++){
    if (customers[i]!=-1) {
      if (unassigned[i]==1) {
	printf(" %d: ",customers[i]);
	if (d[1+NCust][i]+d[i][1+NCust]<TIMELIMIT){

	  numveh++;
	  //tvarcost=tvarcost+ 2*d[fac+NCust][customers[i]]*VOcost/60;
	
	  
	  route1=(ROUTE *) calloc (1,sizeof(ROUTE));
	  route1->cap=Demand[i];
	  route1->time=d[1+NCust][i]+d[i][1+NCust];
	  route1->size= 1;
	  route1->left=i;
	  route1->right=i;
	  route1->seq=(int*)calloc(route1->size,sizeof(int));
	  route1->seq[0]=i;
	  route1->cost=rc[NCust+1][i]+rc[i][NCust+1];
	  printf("With red.cost=%lf  ",route1->cost);
	  
	/*
	if (route1->cost+VFcost-vehicle_dual<-PRECISIONLIM){ //add as a column
	  printf("The route is added as a column \n");
	  label_temp=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
	  label_temp->labeldata.Time=route1->time; 
	  label_temp->labeldata.Cap=route1->cap;
	  label_temp->labeldata.nodeseq=(int *) calloc (NCust, sizeof(int));
	  label_temp->cond='C';
	  
	  for (i=0;i<route1->size;i++) {
	    label_temp->labeldata.nodeseq[route1->seq[i]]=i+1;
	  }
	  label_temp->prev=0; 
	  label_temp->next=label_list;
	  if (label_list)		 
	    label_list->prev=label_temp;
	  label_list=label_temp;
	  label_temp=0;
	}
	*/
	  route1->next=0;
	  route1->prev=0;
	  route1->next= RouteList;
	  if (RouteList)
	    RouteList->prev=route1;
	  RouteList=route1;
	} //if timelimit
      }
      
    }
    //  else 
    // break;
  }
  printf("]\n");
  printf("number of paths is %d \n",numveh);
  

 
  value=(double *)calloc(numveh+1,sizeof(double));
  weight=(int *)calloc(numveh+1,sizeof(int));

  value[0]=0;
  weight[0]=0;
  
  i=0;
  route=RouteList;

  while (route) {

    i++;
    value[i]=-route->cost;
    weight[i]=route->time;
    route=route->next;
  }

  //  for (i=0;i<numveh+1;i++){
  //printf("value[%d]=%lf, weight[%d]=%d \n",i,value[i],i,weight[i]);
  //}
  //printf("total variable cost for fac %d is %lf \n",fac,tvarcost);

  knapsackSoln=(int *)calloc (numveh+1,sizeof(int));

  add_new_col=KnapsackSolnDynProg (value, weight, numveh, TIMELIMIT,vehicle_dual,VFcost,knapsackSoln) ;

  if (add_new_col==1){
    printf("A pairing is constructed from routes \n");
    label_temp=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
    label_temp->cond='C';
    label_temp->labeldata.Time=0; 
    label_temp->labeldata.Cap=0;
    label_temp->labeldata.nodeseq=(int *) calloc (NCust, sizeof(int));
    t=0;
    route=RouteList;
    for (i=1;i<numveh+1;i++){
      if (knapsackSoln[i]==1){
	label_temp->labeldata.Time=label_temp->labeldata.Time+route->time; 
	label_temp->labeldata.Cap=label_temp->labeldata.Cap+route->cap;
	
	for (k=0;k<route->size;k++) {
	  label_temp->labeldata.nodeseq[route->seq[k]]=t+1;
	  t++;
	}
	t++;
      }
      route=route->next;
    }
    label_temp->prev=0; 
    label_temp->next=label_list;
    if (label_list)		 
      label_list->prev=label_temp;
    label_list=label_temp;
    label_temp=0;

  }


  
  label_temp=label_list;
  while (label_temp) {
    printf("time=%d \n",label_temp->labeldata.Time);
    label_temp=label_temp->next;
  }
   
  *ColumnLabels=label_list;
  
  
 

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

  free(value);
  free(weight);
  value=0;
  weight=0;
  
  //printf("total variable cost + bin cost for fac %d is %lf \n",fac,tcost);
  //  return(tcost);   
}





int heur_column_generation_main(int **d, double **rc,int fac,double radius, double v_dual, LABEL_NODE **ColList) {
  //radius must be <=0.5 >=0

  int i,j;
  int *customers;
  double ThresholdDist;
  double TotalCost;
  // ROUTE *List;
  //int  totalnumroute;
  LABEL_NODE *LabelList, *temp;
  

  //Pick a set of customers to run the clark and wright algorithm.
  ThresholdDist=radius*TIMELIMIT;
  
  customers=(int *) calloc (NCust,sizeof(int));
  
  for (i=0;i<NCust;i++) {
    if (d[NCust][i]>ThresholdDist)
      customers[i]=-1;
    else
      customers[i]=i;
  }
  
  //CALL CLARK-WRIGHT ALGORITHM 

  LabelList=*ColList;
  CWAlgorithm(d,rc,customers, Demand, TIMELIMIT, VCap, fac,NCust,VFcost, VOcost, &LabelList,v_dual);
  


  *ColList=LabelList;
  LabelList=0;

  
  
  if (customers)
    free(customers);
  
  
  customers=0;
  
  if (*ColList)
    return(1);
  else 
    return(0);


}
