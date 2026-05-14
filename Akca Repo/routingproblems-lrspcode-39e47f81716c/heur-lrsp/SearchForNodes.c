#include "header.h"
int search_for_nodes (ColumnInfo *Pair_Arr,int pairNO,int n1,int n2,int oper){
  
  //oper=1: branching rule is "if n1 or n2 then n1->n2" "connected" look for if only node1, or only node2 or different order
  //oper=0: look for node1 and node2 are together, since they are not allowed
  
  ColumnInfo *column;
  int success,mark,d,mindiff;
   
  column=Pair_Arr;
  success=0;
  while (column){
    if (column->Name==pairNO){
      success=1;
      break;
    }//if
    column=column->next;
  }//while
   
  if (!success) {//cannot find the column
    fprintf(stdout,"ERROR:Cannot find the pair in the column pool!\n");
    exit(9);
  }//if not success

  if (oper) { //look for CONNECTED, search for only n1, only n2, or not n1->n2

    if (n1==source){
      if (column->sequence[n2]==1)
	mark=0;
      else 
	mark=1;
    } //n1=source
    
    else if (n1==sink){ 
      if (column->sequence[n2]<=1)
	mark=1;
      else {
	mark=1;
	mindiff=4*NCust;
	for (d=0;d<NCust;d++){
	  if ((d!=n2)&&(column->sequence[d]>0)&&(column->sequence[d]<column->sequence[n2])){
	    if (column->sequence[n2]-column->sequence[d]<mindiff)
	      mindiff=column->sequence[n2]-column->sequence[d];
	  }
	}//for
	if (mindiff>=2) {
	  mark=0;
	}
      } //else
      
    } //n1==sink
  
    else if (n2==sink){
      if (column->sequence[n1]==0)
	mark=1;
      else {
	mark=1;
	mindiff=4*NCust;
	for (d=0;d<NCust;d++){
	  if ((d!=n1)&&(column->sequence[d]>0)&&(column->sequence[d]>column->sequence[n1])){
	    if (column->sequence[d]-column->sequence[n1]<mindiff)
	      mindiff=column->sequence[d]-column->sequence[n1];
	    
	  }
	}//for
 	if (mindiff>=2) {
	  mark=0;
	}
      } //else
    } //if n2==sink
    
    else{  //n1 and n2 are customer nodes
      if (column->sequence[n1]+column->sequence[n2]==0)
	mark=0;
      else if ((column->sequence[n1]>0)&&(column->sequence[n2]==0))
	mark=1;
      else if ((column->sequence[n1]==0)&&(column->sequence[n2]>0))
	mark=1;
      else{
	if ((column->sequence[n1]<column->sequence[n2])&&(column->sequence[n2]-column->sequence[n1]==1))
	  mark=0;
	else 
	  mark=1;
      }
    } // if n1 and n2 are customer nodes
    
  }//if oper
  
  else { //if oper=0, mark if path1->path2 is not allowed

    if (n1==source){
      if (column->sequence[n2]==1)
	mark=1;
      else 
	mark=0;
    } //n1=source
   
    else if (n1==sink){ 
      if (column->sequence[n2]<=1)
	mark=0;
      else {
	mark=0;
	mindiff=4*NCust;
	for (d=0;d<NCust;d++){
	  if ((d!=n2)&&(column->sequence[d]>0)&&(column->sequence[d]<column->sequence[n2])){
	    if (column->sequence[n2]-column->sequence[d]<mindiff)
	      mindiff=column->sequence[n2]-column->sequence[d];
	  }
	}//for
	if (mindiff==2) {
	  mark=1;
	  
	}
 
      } //else
      
    } //n1==sink 
   
    else if (n2==sink){
      if (column->sequence[n1]==0)
	mark=0;
      else {
	mark=0;
	mindiff=4*NCust;
	for (d=0;d<NCust;d++){
	  if ((d!=n1)&&(column->sequence[d]>0)&&(column->sequence[d]>column->sequence[n1])){
	    if (column->sequence[d]-column->sequence[n1]<mindiff)
	      mindiff=column->sequence[d]-column->sequence[n1];
	    
	  }
	}//for
 	if (mindiff>=2) {
	  mark=1;
	}
      } //else
    } //if n2==sink
    
    else{  //n1 and n2 are customer nodes
      if (column->sequence[n1]+column->sequence[n2]==0)
	mark=0;
      else if ((column->sequence[n1]>0)&&(column->sequence[n2]==0))
	mark=0;
      else if ((column->sequence[n1]==0)&&(column->sequence[n2]>0))
	mark=0;
      else{
	if ((column->sequence[n1]<column->sequence[n2])&&(column->sequence[n2]-column->sequence[n1]==1))
	  mark=1;
	else 
	  mark=0;
      }
    } // if n1 and n2 are customer nodes
     
  }//else oper==0
 
  //if mark=1, take the index of the variable to set to zero
  //if mark=0, do not take the index 

  /*
  if (mark==1)
    fprintf(stderr,"mark=%d \n",mark); 
  */
  return(mark);
}	
