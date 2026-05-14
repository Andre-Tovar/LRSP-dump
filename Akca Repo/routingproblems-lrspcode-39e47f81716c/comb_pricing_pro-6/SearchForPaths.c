#include "header.h"
int search_for_nodes (  ColumnInfo *Pair_Arr,int pairNO,int p1,int p2,int oper){
  
  //oper=1: look for node1->node2 is required
  //oper=0: look for node1 and node2 not together
  
  ColumnInfo *column;
  int success,mark,d,hasP1,hasP2,P1_P2;
  
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
    fprintf(stderr,"ERROR:Cannot find the pair in the column pool!\n");
    exit(9);
  }//if not success

  if (oper) { //look for CONNECTED
    hasP1=0;
    hasP2=0;
    P1_P2=0;
    mark=0;
    /*    
    fprintf(stderr,"Path:[ ");
    for (d=0;d<MAX_PAIRSZ;d++){
      fprintf(stderr,"%d  ",column->seq[d]);
      if (column->seq[d]==-1)
	break;
    } //for
    fprintf(stderr,"]\n");
    */

    for (d=0;d<MAX_PAIRSZ;d++){
      if (column->seq[d]!=-1){
	if (column->seq[d]==p2){
	  if (!hasP1){ //if Path2 is found but not Path1 before-MARK it
	    mark=1; //mark to get column
	    hasP2=1;
	    break;
	  } //!hasP1
	  else if (column->seq[d-1]==p1){
	    mark=-1; //mark not to take column
	    hasP2=1;
	    break;
	  } //else if col->seq[d-1]==p1
	  else {
	    mark=1;  //mark to take column
	    hasP2=1;
	    break;
	  }//else
	} //if p2
	else if (column->seq[d]==p1)
	  hasP1=1;
      }//if col->seq!=-1
      else
	break;
    }//for d
      
    if (mark==0){ //no P1 and P2
      if (!hasP1){
	//fprintf(stderr,"No path1 or path2 \n");
	mark=-1;
      }
      else
	mark=1; //only path1 but no path2
    }//if
     
    //mark=1 means set the column to zero
    //mark=-1 means do nothing
 
  }//if oper
  
  else { //if oper=0, mark if path1->path2 is not required
    mark=-1;
    //print the paths in pair

    /*    
    fprintf(stderr,"Path:[ ");
    for (d=0;d<MAX_PAIRSZ;d++){
      fprintf(stderr,"%d  ",column->seq[d]);
      if (column->seq[d]==-1)
	break;
    } //for
    fprintf(stderr,"]\n");
    */

    for (d=0;d<MAX_PAIRSZ-1;d++){
      if (column->seq[d]!=-1){
	if ((column->seq[d]==p1)&&(column->seq[d+1]==p2)){
	  mark=1; //mark to get column
	  break;
	} //if
      }//if not -1
      else
	break;
    }//for d

    /*      
   if (mark==-1) //no P1->P2
     fprintf(stderr,"No path1->path2, do not set to zero \n");
   else
     fprintf(stderr,"path1->path2,set to zero \n");
    */

   //mark=1 means set the column to zero
   //mark=-1 means do nothing
   
  }//else oper==0
 
  /*
  if (mark==1)
    fprintf(stderr,"mark=%d \n",mark); 
  */
  return(mark);
}	
