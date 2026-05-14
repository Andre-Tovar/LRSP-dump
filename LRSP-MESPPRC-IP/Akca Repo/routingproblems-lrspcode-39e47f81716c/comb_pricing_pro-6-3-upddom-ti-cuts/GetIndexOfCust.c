#include "header.h"
int get_variable_index (ColumnInfo *column, int n1,int n2,int oper){
  
  //oper=1: branching rule is "if n1 or n2 then n1->n2" "connected" look for if only node1, or only node2 or different order
  //oper=0: look for node1 and node2 are together, since they are not allowed
  
  //ColumnInfo *column;
  int mark;
   
  if (oper) { //look for CONNECTED, search for only n1, only n2, or not n1->n2

    //n1 and n2 are customer nodes
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
    
  }//if oper
  
  else { //if oper=0, mark if path1->path2 is not allowed
    
    //n1 and n2 are customer nodes
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
    
  }//else oper==0
 
  if (mark) 
    return(column->varindex);
  else
    return(-1);

}	
