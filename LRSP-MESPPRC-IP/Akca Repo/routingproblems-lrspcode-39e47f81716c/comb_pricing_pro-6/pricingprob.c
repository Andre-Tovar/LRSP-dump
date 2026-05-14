
#include "header.h"


int Generate_Columns(int **d, double **rc, int *Demand,int source, int sink, LABEL_NODE **ColumnList, double v_dual, int maxnumlabels,int fac) {

  int found,set_threshold,SolveExact;
  LABEL_NODE *List;

  List=*ColumnList;
  found=0;

  if (DoCWHeurColumnGen[fac]==1) {
    printf("Solving CW-HEUR-PRICICING PROBLEM for facility %d \n",fac);
    found=heur_column_generation_main(d, rc,fac,0.5,v_dual, &List); 
    if (found==0) {
      DoCWHeurColumnGen[fac]=0;
      printf("NO COLUMN is found by solving CW-HEUR-PRICICING PROBLEM for facility %d \n",fac);
    }
    else {
      *ColumnList=List;
      List=0;
      return(found);
    }
  }

  if (found==0){

    while (max_num_labels[fac]<=MAXNUMLABELS){
      set_threshold=1;
      SolveExact=0;
      printf("Solving PRICICING PROBLEM with max_num_labels=%d for facility %d \n",max_num_labels[fac],fac);
      found=ESPRC (d,rc,Demand, source, sink, &List, v_dual, max_num_labels[fac], set_threshold);
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
    
    if (found==0)
      printf("NO COLUMN is found by solving PRICING PROBLEM with THRESHOLD for facility %d \n",fac);

    set_threshold=0;
    printf("Solving EXACT PRICICING for facility %d \n",fac);
    found=ESPRC (d,rc,Demand, source, sink, &List, v_dual, max_num_labels[fac], set_threshold);
    if (found==0)
      max_num_labels[fac]=MAXNUMLABELS+2;

    *ColumnList=List;
    List=0;
    return(found);
    
  }



return(found);


}




