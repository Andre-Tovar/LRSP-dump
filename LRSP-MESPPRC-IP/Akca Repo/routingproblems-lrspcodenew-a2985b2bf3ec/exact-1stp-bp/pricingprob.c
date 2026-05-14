
#include "header.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

int Generate_Columns(double **d, double **rc, int *Demand,int source, int sink, LABEL_NODE **ColumnList, double v_dual, int maxnumlabels,int fac,int RyanFoster, int pri_for_1_fac,int Choose_Cust_Set,int *CustomerSet, int SetSize) {

  int found,set_threshold,SolveExact;
  LABEL_NODE *List;

  List=*ColumnList;
  found=0;

  if (DoCWHeurColumnGen[fac]==1) {
    // printf("Solving CW-HEUR-PRICICING PROBLEM for facility %d \n",fac);
    found=heur_column_generation_main(d, rc,fac,0.5,v_dual, &List); 
    if (found==0) {
      DoCWHeurColumnGen[fac]=0;
      //printf("NO COLUMN is found by solving CW-HEUR-PRICICING PROBLEM for facility %d \n",fac);
    }
    else {
      *ColumnList=List;
      List=0;
      return(found);
    }
  }

  if (found==0){
    if (Choose_Cust_Set){ //solve exact
      if (SetSize == CUSTSETSIZE1){
	set_threshold=0;
	printf("Solving EXACT PRICICING for facility for subset of customers %d \n",fac);
	found=ESPRC(d,rc,Demand, source, sink, &List, v_dual, max_num_labels[fac], set_threshold,RyanFoster,pri_for_1_fac,Choose_Cust_Set,CustomerSet);
	*ColumnList=List;
	List=0;
	return(found);
      }
      else if (SetSize == CUSTSETSIZE2){
	set_threshold=1;
	printf("Solving PRICICING for facility for subset of customers with LL = 50%d \n",fac);
	found=ESPRC(d,rc,Demand, source, sink, &List, v_dual, 50, set_threshold,RyanFoster,pri_for_1_fac,Choose_Cust_Set,CustomerSet);
	*ColumnList=List;
	List=0;
	return(found);
      }
      else if (SetSize == CUSTSETSIZE3){
	set_threshold=1;
	printf("Solving PRICICING for facility for subset of customers with LL = 30%d \n",fac);
	found=ESPRC(d,rc,Demand, source, sink, &List, v_dual, 30, set_threshold,RyanFoster,pri_for_1_fac,Choose_Cust_Set,CustomerSet);
	*ColumnList=List;
	List=0;
	return(found);
      }
      else if (SetSize == CUSTSETSIZE4){
	set_threshold=1;
	printf("Solving PRICICING for facility for subset of customers with LL = 20%d \n",fac);
	found=ESPRC(d,rc,Demand, source, sink, &List, v_dual, 20, set_threshold,RyanFoster,pri_for_1_fac,Choose_Cust_Set,CustomerSet);
	*ColumnList=List;
	List=0;
	return(found);
      }
    
    }

    while (max_num_labels[fac]<=MAXNUMLABELS){
      set_threshold=1;
      SolveExact=0;
      printf("Solving PRICICING PROBLEM with max_num_labels=%d for facility %d \n",max_num_labels[fac],fac);
      found=ESPRC(d,rc,Demand, source, sink, &List, v_dual, max_num_labels[fac], set_threshold, RyanFoster,pri_for_1_fac,Choose_Cust_Set,CustomerSet);
      if (found==0){
	max_num_labels[fac]=2*max_num_labels[fac];
	break;
      }
      else {
	*ColumnList=List;
	List=0;
	return(found);
      }
    }
    
    // if (found==0)
    //printf("NO COLUMN is found by solving PRICING PROBLEM with THRESHOLD for facility %d \n",fac);
   
    set_threshold=0;
    printf("Solving EXACT PRICICING for facility %d \n",fac);
    found=ESPRC(d,rc,Demand, source, sink, &List, v_dual, max_num_labels[fac], set_threshold,RyanFoster,pri_for_1_fac,Choose_Cust_Set,CustomerSet);
    if (found==0)
      max_num_labels[fac]=MAXNUMLABELS+2;

    *ColumnList=List;
    List=0;
    return(found);
    
    //return(found);
  }



return(found);


}




