// update reduce list !
//Update domination
//Update second ESPPRC
//Update first ESPPRC

#include "header.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
# define MyEPS 0.000001

double compare(LABEL_NODE *a, LABEL_NODE *b) {
    return (a->labeldata.Cost - b->labeldata.Cost);
}


LABEL_NODE *reduceListWithCondN(LABEL_NODE *list, int maxnum, LABEL_NODE *Ptr, int pointer) {

  LABEL_NODE *temp,*temp2, *lastnode;
  int count;

  // printf("In reduceList\n");
  temp=list;

  count=0;

  while (temp){
    //  printf("count=%d \n",count);
    if (temp->cond=='N')
      count++;

    temp=temp->next;
    if (count>=maxnum) {
      // printf("break since count = %d >= maxnum =%d\n",count,maxnum);
     break;
    }
  }

  if (!temp)
    return list;


  lastnode=temp->prev;
  //if (pointer==1)
  // printf("last nodes 's cost =%lf \n",lastnode->labeldata.Cost);

  if (pointer==0){
    if (temp){
      (temp->prev)->next=0;
      temp->prev=0;
    }
    
    
    //delete the rest starting from label temp
    
    while (temp) {
      temp2=temp->next;
      if (temp2)      
	temp2->prev=0;
      temp->prev=0;
      temp->next=0;
      
      free(temp->labeldata.nodeseq);
      temp->labeldata.nodeseq=0;
      free(temp);
      temp=temp2;
    } //while
    
  }
  else{
    
    if (temp){
      (temp->prev)->next=0;
      temp->prev=0;
    }
    
    while (temp) {
      
	temp2=temp->next;
	if (temp2)      
	  temp2->prev=0;
	temp->next=0;
	if (temp!=Ptr){
	  temp->prev=0;
	  free(temp->labeldata.nodeseq);
	  temp->labeldata.nodeseq=0;
	  free(temp);
	  temp=temp2;
	}
	else {
	  //  printf("don't delete it is pointer, cost= %lf \n", temp->labeldata.Cost);
	  if(lastnode)
	    lastnode->next=temp;
	  temp->prev=lastnode;
	  lastnode=temp;
	  temp=temp2;
	}
	
    } //while
  }

  return list;
  
} 





//To sort the label list based on the reduced cost see function cmp 
//from:  http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
LABEL_NODE *SORTLIST(LABEL_NODE *list) {

  LABEL_NODE *p, *q, *e, *tail,*oldhead;;
  int insize, nmerges, psize, qsize, i;
  int is_circular, is_double;

  is_double=1;
  is_circular=0;

    /*
     * Silly special case: if `list' was passed in as NULL, return
     * NULL immediately.
     */
  if (!list)
    return NULL;

    insize = 1;

    while (1) {
        p = list;
	oldhead = list;		       /* only used for circular linkage */
        list = NULL;
        tail = NULL;

        nmerges = 0;  /* count number of merges we do in this pass */

        while (p) {
            nmerges++;  /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
		if (is_circular)
		    q = (q->next == oldhead ? NULL : q->next);
		else
		    q = q->next;
                if (!q) break;
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {

                /* decide whether next element of merge comes from p or q */
                if (psize == 0) {
		    /* p is empty; e must come from q. */
		    e = q; q = q->next; qsize--;
		    if (is_circular && q == oldhead) q = NULL;
		} else if (qsize == 0 || !q) {
		    /* q is empty; e must come from p. */
		    e = p; p = p->next; psize--;
		    if (is_circular && p == oldhead) p = NULL;
		} else if (compare(p,q) <= 0) {
		    /* First element of p is lower (or same);
		     * e must come from p. */
		    e = p; p = p->next; psize--;
		    if (is_circular && p == oldhead) p = NULL;
		} else {
		    /* First element of q is lower; e must come from q. */
		    e = q; q = q->next; qsize--;
		    if (is_circular && q == oldhead) q = NULL;
		}

                /* add the next element to the merged list */
		if (tail) {
		    tail->next = e;
		} else {
		    list = e;
		}
		if (is_double) {
		    /* Maintain reverse pointers in a doubly linked list. */
		    e->prev = tail;
		}
		tail = e;
            }

            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }
	if (is_circular) {
	    tail->next = list;
	    if (is_double)
		list->prev = tail;
	} else
	    tail->next = NULL;

        /* If we have done only one merge, we're finished. */
        if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
            return list;

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
}
  


//the function to eliminate the dominated labels:

int dominate (LABEL_NODE **list1,LABEL_NODE **list2,int maxlabelsize, int deletelabels, LABEL_NODE *Ptr, int pointer){
 

  LABEL_NODE *temp1,*temp2,*a1,*a2; 
 int passed,allsmall,i,updated,changed,x1,x2;
 LABEL_NODE *zel_n,*zel_p,*last_node;
 // int numList1,numList2,sizeallList;
 LABEL_NODE *sortedlist,*limitedList;

 temp1=*list1;
 temp2=*list2;

 a1=temp1;
 


 if (!temp1)
   last_node=temp1;
 else {
   while (a1){
     //numList1++;
     a2=a1;
     a1=a1->next;
   }
   last_node=a2;
 }




 //add two lists:
 if (last_node){
   last_node->next=temp2;
   temp2->prev=last_node;
 }
 else {
   temp2->prev=0;
   temp1=temp2;
 }



 passed=0;
 a1=temp1;
 while (a1){
   updated=0;
   if ((last_node) && (!passed))
     a2=last_node->next;
   else
     a2=a1->next;
    while (a2){
     
      allsmall=1;
      x1=((a1->labeldata.Time<=a2->labeldata.Time+MyEPS)&& (a1->labeldata.Cost<=a2->labeldata.Cost+MyEPS)&& (a1->labeldata.unreach<=a2->labeldata.unreach)&&(a1->labeldata.Cap<=a2->labeldata.Cap));
      x2=((a1->labeldata.Time>=a2->labeldata.Time-MyEPS)&& (a1->labeldata.Cost>=a2->labeldata.Cost-MyEPS)&& (a1->labeldata.unreach>=a2->labeldata.unreach)&&(a1->labeldata.Cap>=a2->labeldata.Cap));
      if (x1 || x2 ){
	if (x1){
	  // printf("x1 is selected\n");
	  // printf("allsmall");
	  allsmall=1;
	  //printf("allsmall");
	  //printf("NCust=%d", NCust);
	  for (i=0;i<NCust;i++) {
	    // printf("%d ",i);
	    if (((a1->labeldata.nodeseq[i]>0)||(a1->labeldata.nodeseq[i]<0))&&(a2->labeldata.nodeseq[i]==0)){
	      allsmall=0;
	      break;
	    }
	  }//for
	  if (allsmall){

	    // printf("all small \n");
	    if ((pointer==0)||((pointer==1)&&(a2!=Ptr))){
	    //delete a2;

              zel_p=a2->prev;
	      zel_n=a2->next;
	      a2->prev=0;
	      a2->next=0;
	    
	      free(a2->labeldata.nodeseq);
	      a2->labeldata.nodeseq=0;
	      free(a2); 
	      a2=0;
	      a2=zel_n;
	      if (zel_p)
		zel_p->next=a2;
	      if (a2)
		a2->prev=zel_p;
	    }
	  }//if allsmall, deleting a2
	  else
	    a2=a2->next;
	}//if x1
	
	else if (x2){
	 
	  allsmall=1;
	  for (i=0;i<NCust;i++) {
	    if (((a2->labeldata.nodeseq[i]>0)||(a2->labeldata.nodeseq[i]<0))&&(a1->labeldata.nodeseq[i]==0)){
	      allsmall=0;
	      break;
	    }
	  }//for
	  if (allsmall){
	    //delete a1:

	    if ((pointer==0)||((pointer==1)&&(a1!=Ptr))){
	      if (a1->prev)
		zel_p=a1->prev;
	      else
		zel_p=0;
	      zel_n=a1->next;
	      if ((last_node)&& (a1==last_node)){
		if (zel_p)
		  last_node=zel_p;
		else
		  last_node=0;
		passed=1;
	      }
	      a1->prev=0;
	      a1->next=0;
	   
	      free(a1->labeldata.nodeseq);
	      a1->labeldata.nodeseq=0;
	      free(a1); 
	      a1=0;
	      a1=zel_n;
	      if (zel_p){
		zel_p->next=a1;
		if (a1)
		  a1->prev=zel_p;
	      }
	      else {
		temp1=a1;
		if (a1)
		  a1->prev=0;
	      }
	      updated=1;
	      break;
	    }
	  } //if allsmall
	  else
	    a2=a2->next;
	}//else if x2 
      }//if x1||x2
    
      else
	a2=a2->next;

    } //while a2


    if (!updated){
      if ((last_node)&&(a1==last_node))
	passed=1;      
      a1=a1->next;
    }//if updated

 }//while a1
 
 if ((last_node)&& (last_node->next))
   changed=1;
 else if ((!last_node) && (temp1))
   changed=1;
 else
   changed=0;



 // printf("Before list is sorted\n");
 


 
 // printf("List is sorted \n");
 
 
 if (deletelabels==1) {
   sortedlist=SORTLIST(temp1);
   //  limitedList=reduceList(sortedlist,maxlabelsize,Ptr,pointer);
   limitedList=reduceListWithCondN(sortedlist,maxlabelsize,Ptr,pointer);
   *list1=limitedList;
   // limitedList=reduceList(temp1,maxlabelsize);
   //*list1=sortedlist;
 }
 else
   //*list1=sortedlist;
    *list1=temp1;
 *list2=0;
 


 return (changed);
 
}

int Generate1RandomNum (int begin, int end){

return((int)(rand()%(end - begin + 1))+ begin);

}

int AddLists (LABEL_NODE **list1, LABEL_NODE **list2) {

  LABEL_NODE *a1,*a2,*temp,*last_node,*temp1;

  a1=*list1;
  a2=*list2;
  last_node=0;

  if ((!a1) && (!a2)){
    *list1=0;
    *list2=0;
    return(1);
  }

  else if (!a1) {
    *list1=a2;
    *list2=0;
    a1=0;
    a2=0;
    return(1);
  }
  else if (!a2){
    *list1=a1;
    *list2=0;
    a1=0;
    a2=0;
    return(1);
  }
  else { 
    
    temp=a1;
    if (!temp) {
      last_node=temp;
      printf("ERROR in AddList function list1 is empty \n");
      exit(9);
    }
    else {
      while (temp){
	temp1=temp;
	temp=temp->next;
      }
      last_node=temp1;
    }
  
    //add two lists:
    if (last_node){
      last_node->next=a2;
      if (a2)
	a2->prev=last_node;
      else {
	printf("ERROR in AddList function list2 is empty \n");
	exit(9);
      }
    }

    *list1=a1;
    *list2=0;
    return(1);
  }


}

   
int ESPPRC_Pairing (LABEL_NODE *Sink_Labels, int numlabels, double tmin, LABEL_NODE **PairingList, double v,int maxnumlabels, int set_threshold,double **d, int sink,int RyanFoster) 
{

  int i,j,k,vi;
  LABEL_NODE **LabelList,*temp, **NodePtr, *F, *temp1; //, *ptemp;
  int *E;
  int empty,enter,changed,success;
  double costmin;

  LabelList = (LABEL_NODE **) calloc (numlabels, sizeof(LABEL_NODE *)); 
  NodePtr = (LABEL_NODE **) calloc (numlabels, sizeof(LABEL_NODE *)); 

  E=(int *) calloc (numlabels, sizeof(int));

  costmin=Sink_Labels->labeldata.Cost;
  if (costmin>MyEPS)
    costmin=0;


  /*
  printf("\n In Second ESSPRC Before start\n");
  temp=Sink_Labels;
  while (temp){
    printf("[%lf, %d, %lf, %d,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->labeldata.unreach);
    for (j=0;j<NCust;j++) {
      //if (temp->labeldata.nodeseq[j]>0)
      printf("%d:%d  ",j,temp->labeldata.nodeseq[j]);
    }
    printf(")]\n");
    temp=temp->next;
  }
  */

  temp=Sink_Labels;
  for (i=0;i<numlabels;i++){
    LabelList[i]=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
    LabelList[i]->labeldata.Cap=0;
    LabelList[i]->labeldata.Time=temp->labeldata.Time;
    LabelList[i]->labeldata.Cost=temp->labeldata.Cost;
    LabelList[i]->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
    LabelList[i]->labeldata.unreach=0;

    for (j=0;j<NCust;j++) {
      if ((temp->labeldata.nodeseq[j]>0)||(temp->labeldata.nodeseq[j]==-1)){
	LabelList[i]->labeldata.nodeseq[j]=temp->labeldata.nodeseq[j];
	LabelList[i]->labeldata.unreach++;
      }
      else if ((temp->labeldata.nodeseq[j]==-2)){
	if ((RyanFoster==0)&&(temp->labeldata.Time+d[sink][j]+d[j][sink]>TIMELIMIT)) {
	  LabelList[i]->labeldata.nodeseq[j]=-1;
	  LabelList[i]->labeldata.unreach++;
	}
	else
	  LabelList[i]->labeldata.nodeseq[j]=0;
      }
      else
	LabelList[i]->labeldata.nodeseq[j]=temp->labeldata.nodeseq[j];
    }


    LabelList[i]->labeldata.count=temp->labeldata.count;
    
    // LabelList[i]->labeldata.first_node=i; 
    LabelList[i]->next=0; 
    LabelList[i]->prev=0;

    if(costmin > LabelList[i]->labeldata.Cost)
      costmin=LabelList[i]->labeldata.Cost;

    if (LabelList[i]->labeldata.Cost>MyEPS){
      if (LabelList[i]->labeldata.Cost+VFcost-v>-PRECISIONLIM)
	LabelList[i]->cond='P';
      else
	LabelList[i]->cond='C';
      E[i]=-1;
    }

    else if (LabelList[i]->labeldata.Time+tmin>TIMELIMIT) {
      if (LabelList[i]->labeldata.Cost+VFcost-v>-PRECISIONLIM)
	LabelList[i]->cond='P';
      else
	LabelList[i]->cond='C';
      E[i]=-1;
    }
    else if (LabelList[i]->labeldata.unreach>=NCust){
      if (LabelList[i]->labeldata.Cost+VFcost-v>-PRECISIONLIM)
	LabelList[i]->cond='P';
      else
	LabelList[i]->cond='C';
      E[i]=-1;
    }
    else {
      LabelList[i]->cond='N';//new label
      E[i]=i;
    }

    NodePtr[i]=LabelList[i];
    temp=temp->next;

  }//for i
  
  /*
  printf("Label List for the new network \n");
  for (i=0;i<numlabels;i++){
    temp=LabelList[i];
    printf("[%lf, %d, %lf, %d, %c(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->labeldata.unreach,temp->cond);
    for (j=0;j<NCust;j++) {
      //if (temp->labeldata.nodeseq[j]>0)
      printf("%d:%d  ",j,temp->labeldata.nodeseq[j]);
    }
    printf(")]\n");
  }
  */

  /*
  printf("numlabels=%d, tmin=%lf  Before Pairing ESPPRC: \n",numlabels,tmin);
  for (i=0;i<numlabels;i++){
    temp=LabelList[i];
    printf("i=%d, LabelList[i].cost=%lf \n",i,temp->labeldata.Cost);
    while (temp){
      printf("[%lf, %d, %lf, %c,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->cond);
      for (j=0;j<NCust;j++) {
	//if (temp->labeldata.nodeseq[j] >0)
	  printf("%d:%d  ",j,temp->labeldata.nodeseq[j]);
      }
      printf(")]\n");
      //temp->labeldata.first_node=labelnumber;
      temp=temp->next;
    }
  }    
  */

  empty=0; //shows the list of E is empty or not?
  vi=-1;

  for (i=0;i<numlabels;i++) {
       if (E[i]==i) {
	 vi=i;             //take vi to be the node to be treated     
	 break;
       }
  }

  while (!empty){    //&& (NumSoln<500)){  //IF THE SET IS NOT EMPTY:
     
    if (vi!=-1) {
      if  (LabelList[vi]){ //if set E is not empty
	

	for (j=vi+1;j<numlabels;j++) {
	  //	  if ((vi==40) && (j==139))
	  // printf("vi =%d, j=%d \n",vi,j);
	  
	  // success=0;
	  if(NodePtr[j]->cond=='N'){
	    temp=LabelList[vi];
	    F=0;
	    while (temp) { //for all labels of vi:
	      /*
		printf("temp->Cost=%lf, temp->Time=%lf, temp->Cap=%d, temp->nodeseq: ",temp->labeldata.Cost, temp->labeldata.Time, temp->labeldata.Cap);
		for (za=0;za<NCust;za++)
		printf("%d:%d ",za, temp->labeldata.nodeseq[za]);
		printf("\n");
	      */
	      
	      if (temp->cond=='N'){
		
		if ((temp->labeldata.Time+NodePtr[j]->labeldata.Time<=TIMELIMIT)) { //if feasible wrt TIMELIMIT
		  enter=1;
		  for (k=0;k<NCust;k++){		      
		    if ((temp->labeldata.nodeseq[k]!=0)&&(NodePtr[j]->labeldata.nodeseq[k]!=0)){
		      if(!((temp->labeldata.nodeseq[k]<0)&&(NodePtr[j]->labeldata.nodeseq[k]<0))) {
			enter=0;
			break;
		      }
		    }
		  }
		  if (enter==1){ //feasible wrt visited nodes
		    // printf("Combine! \n");
		    temp1=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
		    temp1->labeldata.Time=temp->labeldata.Time+NodePtr[j]->labeldata.Time; 
		    temp1->labeldata.Cap=0;
		    temp1->labeldata.Cost=temp->labeldata.Cost+NodePtr[j]->labeldata.Cost;
		    temp1->labeldata.nodeseq=(int *) calloc (NCust, sizeof(int));
		    temp1->labeldata.count=temp->labeldata.count+NodePtr[j]->labeldata.count;
		    //	 temp1->labeldata.unreach=temp->labeldata.unreach+NodePtr[j]->labeldata.unreach;
		    temp1->labeldata.unreach=0;
		    
		    
		    for (k=0;k<NCust;k++) {
		      if ((temp->labeldata.nodeseq[k]>0) && (NodePtr[j]->labeldata.nodeseq[k]==0)){
			temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			temp1->labeldata.unreach++;
		      }
		      else if ((NodePtr[j]->labeldata.nodeseq[k]>0)&&(temp->labeldata.nodeseq[k]==0)){
			temp1->labeldata.nodeseq[k]=NodePtr[j]->labeldata.nodeseq[k]+temp->labeldata.count;
			temp1->labeldata.unreach++;
		      }
		      else if ((temp->labeldata.nodeseq[k]<0) || (NodePtr[j]->labeldata.nodeseq[k]<0)){
			temp1->labeldata.nodeseq[k]=-1;
			temp1->labeldata.unreach++;
		      }
		      // If there is Ryan and foster branching rule, the trainguler inequality is  not satisfied
		      else if ((temp->labeldata.nodeseq[k]==0) && (NodePtr[j]->labeldata.nodeseq[k]==0)){
			if ((RyanFoster==0)&&(temp1->labeldata.Time + d[k][sink]+ d[sink][k] > TIMELIMIT)) {
			  temp1->labeldata.nodeseq[k]=-1;
			  temp1->labeldata.unreach++;
			}
			else
			  temp1->labeldata.nodeseq[k]=0;
		      }
		    }
		    
		    if (temp1->labeldata.Time+tmin>TIMELIMIT) {
		      if (temp1->labeldata.Cost+VFcost-v>-PRECISIONLIM)
			temp1->cond='P';
		      else
			temp1->cond='C';
		    }
		    else if (temp1->labeldata.Cost > MyEPS) {
		      if (temp1->labeldata.Cost+VFcost-v>-PRECISIONLIM)
			temp1->cond='P';
		      else
			temp1->cond='C';
		    }
		    else {  
		      temp1->cond='N';
		      // if (success==0)
		   //success=1;
		    }

		    /*
		      printf("New label \n");
		      printf("temp1->Cost=%lf, temp1->Time=%lf, temp1->cond=%c, temp1->nodeseq: ",temp1->labeldata.Cost, temp1->labeldata.Time, temp1->cond);
		      for (za=0;za<NCust;za++)
		      printf("%d:%d ",za, temp1->labeldata.nodeseq[za]);
		      printf("\n");
		    */
		    
		    if(costmin > temp1->labeldata.Cost)
		      costmin=temp1->labeldata.Cost;
		    
		    
		    temp1->prev=0; //add label to F
		    temp1->next=F;
		    if (F)		 
		      F->prev=temp1;
		    F=temp1;
		  
		    
		  } //feasible wrt visited nodes
		} //if feasible wrt TIMELIMIT

	      }//if cond==N
	      temp=temp->next;
	    }//while temp
	    success=0;
	    if(F){    
	      /*
		printf("Before domination, LabelList[j] \n");
		ptemp=LabelList[j];
		while (ptemp){
		printf("ptemp->Cost=%lf, ptemp->Time=%lf, ptemp->Cap=%d, ptemp->cond=%c, ptemp->nodeseq: ",ptemp->labeldata.Cost,  ptemp->labeldata.Time, ptemp->labeldata.Cap,ptemp->cond);
		for (za=0;za<NCust;za++)
		printf("%d:%d ",za, ptemp->labeldata.nodeseq[za]);
		printf("\n");
		ptemp=ptemp->next;
		}
		printf("Before domination, F \n");
		ptemp=F;
		while (ptemp){
		printf("ptemp->Cost=%lf, ptemp->Time=%lf, ptemp->Cap=%d, ptemp->cond=%c, ptemp->nodeseq: ",ptemp->labeldata.Cost,  ptemp->labeldata.Time, ptemp->labeldata.Cap,ptemp->cond); 
		for (za=0;za<NCust;za++)
		printf("%d:%d ",za, ptemp->labeldata.nodeseq[za]);
		printf("\n");
		ptemp=ptemp->next;
		}
	      */
	      
	      //success=dominate(&LabelList[j],&F,maxnumlabels, set_threshold,NodePtr[j],1);
	      success=dominate(&LabelList[j],&F,maxnumlabels, 0,NodePtr[j],1);
	      
	      //success=AddLists(&LabelList[j],&F);
	      
	      /*
		printf("After domination, LabelList[j] \n");
		ptemp=LabelList[j];
		while (ptemp){
		printf("ptemp->Cost=%lf, ptemp->Time=%lf, ptemp->Cap=%d, ptemp->cond=%c, ptemp->nodeseq: ",ptemp->labeldata.Cost,  ptemp->labeldata.Time, ptemp->labeldata.Cap,ptemp->cond); 
		for (za=0;za<NCust;za++)
		printf("%d:%d ",za, ptemp->labeldata.nodeseq[za]);
		printf("\n");
		ptemp=ptemp->next;
		}
	      */
	    }
	    if (success){
	      if (E[j]!=j)
		E[j]=j;
	    }

	  } //if j->cond=N
	}//for all j
	
	//mark the labels as processed or candidate 
	temp=LabelList[vi];
	while (temp) {
	  if (temp->labeldata.Cost+VFcost-v<-PRECISIONLIM)
	    temp->cond='C';
	  else
	    temp->cond='P';
	  temp=temp->next;
	} //while
	
	E[vi]=-1;
	
      } //(List[vi]
      

      empty=1;
      for (i=vi+1;i<numlabels;i++) {
	if (E[i]==i) {
	  vi=i;             //take vi to be the node to be treated
	  empty=0;     
	  break;
	}
      }

      if (empty==1){
	for (i=0;i<=vi;i++) {
	  if (E[i]==i) {
	    vi=i;             //take vi to be the node to be treated
	    empty=0;     
	    break;
	  }
	}
      }

    } //if vi!=-1

    else
      empty=1;
    
  }//while not empty
 
  /*
  printf("labels:[cost,demand,Time,cond,(path)]\n");
  for (i=0;i<numlabels;i++){
    if ((NodePtr[i]->labeldata.Cost >= -67.9)&&(NodePtr[i]->labeldata.Cost <= -66.9)) {    
      temp=LabelList[i];
      printf("NodePtr[i]->labeldata.Cost= %lf\n",NodePtr[i]->labeldata.Cost);
      while (temp){
	printf("[%lf, %d, %lf, %c,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->cond);
	for (j=0;j<NCust;j++)
	  printf("%d  ",temp->labeldata.nodeseq[j]);
	printf(")]\n");
	temp=temp->next;
      }
      //break;
    }
  }
  
  */
  changed=0;
  if (costmin + VFcost - v < - PRECISIONLIM){
    /*
    //Combine lists, delete dominated ones and sort.
    for (i=1;i<numlabels;i++){
      success=dominate(&LabelList[0],&LabelList[i],maxnumlabels, 0,0,0);
    }
    
    if (set_threshold!=1){
      F=SORTLIST(LabelList[0]);
      LabelList[0]=F;
    }
    */
 
    
    //Combine without domination 
    for (i=1;i<numlabels;i++){
      success=AddLists(&LabelList[0],&LabelList[i]);
    }
/*     F = 0; */
/*     success=dominate(&F, &LabelList[0],maxnumlabels, set_threshold,0,0); */
/*     LabelList[0] = F; */
/*     F = 0; */
 
    F=SORTLIST(LabelList[0]);
    LabelList[0]=F;
    

    if (LabelList[0]->cond=='C')
      changed=1;
    
    *PairingList=LabelList[0];
  }
  else {
    *PairingList=0;
    changed=0;
    for (i=0;i<numlabels;i++){
      temp=LabelList[i];
      while (temp){
	LabelList[i]=temp->next;
	if (LabelList[i])
	  LabelList[i]->prev=0;
	temp->next=0;
	free(temp->labeldata.nodeseq);
	temp->labeldata.nodeseq=0;
	free(temp);
	temp=LabelList[i];
      }
    } //i
  }

  free(E);
  E=0;

  return (changed);
}




int ESPRC (double **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold, int RyanFoster, int pricing_for_one_fac,int Choose_Cust_Set,int *CustomerSet)

{
  int i,j,k,vi;
  int *E;
  int *dem;
  int changed,empty,success,enter;
  //  double min;
  LABEL_NODE **LabelList,*F,*temp,*temp1, *ListBegin; //*temp_next; 
  // int *numLabels;
  int sizeofF, labelnumber;
  double mintime;
  double add_dist;
  //  int za;
  //LABEL_NODE *ptemp;

  // time_t t1,t2;
  //  numLabels=(int *)calloc (NCust+2,sizeof(int));


  LabelList = (LABEL_NODE **) calloc (NCust+2, sizeof(LABEL_NODE *)); 

  dem=(int *) calloc (NCust+2, sizeof (int));
  for (i=0;i<NCust;i++)
    dem[i]=Demand[i];
  dem[source]=0;

  //  dem[sink]=-VCap;
  dem[sink]=0;
  
//intitialization:
  LabelList[source]=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
  
  LabelList[source]->labeldata.Cap=0;
  LabelList[source]->labeldata.Time=0;
  LabelList[source]->labeldata.Cost=0.0;
  LabelList[source]->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
  LabelList[source]->labeldata.count=0;
  LabelList[source]->labeldata.unreach=0;
  LabelList[source]->labeldata.first_node=-1;
  LabelList[source]->next=0; 
  LabelList[source]->prev=0; 
  LabelList[source]->cond='N';//new label
 
  
  if (Choose_Cust_Set==1){
    for (i=0;i<NCust;i++){
      if (CustomerSet[i]==0){
	LabelList[source]->labeldata.nodeseq[i]=-1;
	LabelList[source]->labeldata.unreach++;
      }
    }
  }


  for (i=0;i<NCust+2;i++) {
    if (i!=source) 
      LabelList[i]=0;
  
  }
  
  //node set..   
  E=(int *) calloc (NCust+2, sizeof(int));
  E[source]=source;  //if E[i]=i, i th node is in the list, E, otherwise it is -1:
  E[sink]=-1;
  for (i=0;i<NCust;i++) 
    E[i]=-1;

  empty=0; //shows the list of E is empty or not?
  //NumSoln=0;
 
  //time(&t1);

  while (!empty){    //&& (NumSoln<500)){  //IF THE SET IS NOT EMPTY:
     vi=-1;

     j=Generate1RandomNum(0,NCust+1);
     //j=Generate1RandomNum(0,NCust+2);

     //for (i=j;i<NCust+2;i++) {
     for (i=j;i<NCust+1;i++) {
       if (E[i]==i) {
	 vi=i;             //take vi to be the node to be treated     
	 break;
       }
     }

     if (vi==-1) {
       for (i=0;i<j;i++) {
	 if (E[i]==i) {
	   vi=i;             //take vi to be the node to be treated     
	   break;
	 }
       }
     }

     if (vi!=-1) {
       if  (LabelList[vi]){ //if set E is not empty
	 //printf("vi=%d \n",vi);
	 for (j=0;j<NCust+2;j++) {
	 
	   //if ((set_threshold==0)||(numLabels[j]<threshold)) {
	     if ((j!=vi)&& (j!=source)){
	       if (RyanFoster==0)
		 add_dist=d[vi][j]+d[j][sink];
	       else
		 add_dist=d[vi][j];

	       if ((add_dist<=TIMELIMIT)&&(dem[vi]+dem[j]<=VCap)){
		 //   F = (LABEL_NODE *) calloc (1, sizeof(LABEL_NODE));  
		 F=0;
		 sizeofF=0;
		 //	 if (stepcount==5)
		 //printf("vi=%d -------vj=%d----------------------------\n",vi,j);
		 temp=LabelList[vi];
		 
	

		 while (temp) { //for all labels of vi:
		   /*
		   if (stepcount==5){
		   //print temp
		   printf("will be extended to %d, d[vi][j]=%lf, dem[j]=%d, rc[vi][j]=%lf\n",j,d[vi][j],dem[j],rc[vi][j]);
		   printf("temp->Cost=%lf, temp->Time=%lf, temp->Cap=%d, temp->nodeseq: ",temp->labeldata.Cost, temp->labeldata.Time, temp->labeldata.Cap);
		   for (za=0;za<NCust;za++)
		     printf("%d:%d ",za, temp->labeldata.nodeseq[za]);
		   printf("\n");
		   }
		   */

		   if (temp->cond=='N'){
		     // if ((j==sink)||(temp->labeldata.nodeseq[j]==0)){ //be careful segmentation error might be nodeseq does not have sink  //if not in path
		     enter=0;
		     if (j==sink) {
		       if ((VFcost-v>=0)&&(temp->labeldata.Cost+rc[vi][j]>-PRECISIONLIM))
			 enter=0;
		       else
			 enter=1;
		     }
		     else {
		       if (temp->labeldata.nodeseq[j]==0)
			 enter=1;
		     }

		     // printf("enter=%d\n",enter);

		     if (enter){
		       if (RyanFoster==0)
			 add_dist=d[vi][j]+d[j][sink];
		       else
			 add_dist=d[vi][j];
		       if ((temp->labeldata.Time+add_dist<=TIMELIMIT)&&(temp->labeldata.Cap+dem[j]<=VCap)) { //if a feasible extension
		      
			 // fprintf(stderr,"A label is created for node %d\n",j);
			 temp1=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
			 
			 temp1->labeldata.Time=temp->labeldata.Time+d[vi][j]; 
		
			 //if (temp->labeldata.Cap+dem[j]<0)
			 //temp1->labeldata.Cap=0;
			 //else
			 if (j==sink)
			   temp1->labeldata.Cap=0;
			 else
			   temp1->labeldata.Cap=temp->labeldata.Cap+dem[j];
			 
			 temp1->labeldata.Cost=temp->labeldata.Cost+rc[vi][j];
			 temp1->labeldata.nodeseq=(int *) calloc (NCust, sizeof(int));
			 temp1->labeldata.count=temp->labeldata.count+1;

			 /*
			 if ((temp->labeldata.first_node==-1) || (vi==sink))
			   temp1->labeldata.first_node=j;
			 else
			   temp1->labeldata.first_node=temp->labeldata.first_node;
			 */

			 temp1->labeldata.unreach=temp->labeldata.unreach;
			 for (k=0;k<NCust;k++) {
			   if (k==j) {
			     temp1->labeldata.nodeseq[k]=temp1->labeldata.count;
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			   }
			   else if ((temp->labeldata.nodeseq[k]>0) || (temp->labeldata.nodeseq[k]==-1 )){
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			   }
			   else if ((j==sink) && (temp->labeldata.nodeseq[k]==-2 )){
			     if ((RyanFoster==0)&&(temp1->labeldata.Time+d[k][sink]+d[j][k]>TIMELIMIT))
			       temp1->labeldata.nodeseq[k]=-1;
			     else {
			       temp1->labeldata.nodeseq[k]=0;
			       temp1->labeldata.unreach--;
			     }
			   }
			   else if ((j!=sink) && (temp->labeldata.nodeseq[k]==-2 ))
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];

			   else if ((RyanFoster==0)&&(temp1->labeldata.Time+d[k][sink]+d[j][k]>TIMELIMIT)) { //||(temp1->labeldata.Cap+dem[k]>VCap)) {
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			     temp1->labeldata.nodeseq[k]=-1; //unreachable BECAUSE OF TIMELIMIT
			   }
			   else if ((j!=sink)&&(temp1->labeldata.Cap+dem[k]>VCap)){
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			     temp1->labeldata.nodeseq[k]=-2; //unreachable BECAUSE OF VEHICLE CAPACITY
			   }
			   
			   else
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			 }
			 
			 //For sink node, delete if number of unreachable >= NCust
		
			 //no_zero=0;
			 // if ((j==sink)&&(temp1->labeldata.Cost+VFcost-v>-PRECISIONLIM)){

			 /*
			 if (stepcount==5){  
			 printf("New Label temp1 \n");
			 printf("temp1->Cost=%lf, temp1->Time=%lf, temp1->Cap=%d, temp1->nodeseq: ",temp1->labeldata.Cost, temp1->labeldata.Time, temp1->labeldata.Cap);
			 for (za=0;za<NCust;za++)
			   printf("%d:%d ",za, temp1->labeldata.nodeseq[za]);
			 printf("\n");
			 }
			 */

			 if ((j==sink)&&(temp1->labeldata.Cost>-PRECISIONLIM)&&(VFcost-v>=0)){

			   /*
			   no_zero=1;
			   for (k=0;k<NCust;k++) {
			     if (temp1->labeldata.nodeseq[k]==0){
			       no_zero=0;
			       break;
			     }
			   }
			   */
			   //if (no_zero==1) {
			     free(temp1->labeldata.nodeseq);
			     free(temp1);
			     temp1=0;
			     // }
			 }
			 // if (no_zero==0){
			 else{
			   
			   if (j==sink)
			     temp1->cond='C';
			   else
			     temp1->cond='N';

			   temp1->prev=0; //add label to F
			   temp1->next=F;
			   if (F)		 
			     F->prev=temp1;
			   F=temp1;
			   sizeofF++;
			   //}
			 }
		       } //if feasible extension
		     }//if (j==sink)||(temp->labeldata.nodeseq[j]==0)
		   }//if cond==N
		   temp=temp->next;
		 }//while temp
	       
		 //look whether labels of j is changed or not  
	       
		 success=0;
		 
		 if (F){
		   if (j!=sink){
		     /*
		     if (stepcount==5){
		     printf("Print labels of LabelList[j] and F before domination algorithm\n");
		     printf("LabelList[j]: \n");
		     ptemp=LabelList[j];
		     while(ptemp){
		       printf("ptemp->Cost=%lf, ptemp->Time=%lf, ptemp->Cap=%d, ptemp->nodeseq: ",ptemp->labeldata.Cost, ptemp->labeldata.Time, ptemp->labeldata.Cap);
		       for (za=0;za<NCust;za++)
			 printf("%d:%d ",za, ptemp->labeldata.nodeseq[za]);
		       printf("\n");
		       ptemp=ptemp->next;
		     }
		     printf("F: \n");
		     ptemp=F;
		     while(ptemp){
		       printf("ptemp->Cost=%lf, ptemp->Time=%lf, ptemp->Cap=%d, ptemp->nodeseq: ",ptemp->labeldata.Cost, ptemp->labeldata.Time, ptemp->labeldata.Cap);
		       for (za=0;za<NCust;za++)
			 printf("%d:%d ",za, ptemp->labeldata.nodeseq[za]);
		       printf("\n");
		       ptemp=ptemp->next;
		     }
		     }
		     */
		     success=dominate(&LabelList[j],&F,maxnumlabels, set_threshold,0,0);
		     
		     /*
		     if (stepcount==5){
		     printf("After domination\n");
		     ptemp=LabelList[j];
		     while(ptemp){
		       printf("ptemp->Cost=%lf, ptemp->Time=%lf, ptemp->Cap=%d, ptemp->nodeseq: ",ptemp->labeldata.Cost, ptemp->labeldata.Time, ptemp->labeldata.Cap);
		       for (za=0;za<NCust;za++)
			 printf("%d:%d ",za, ptemp->labeldata.nodeseq[za]);
		       printf("\n");
		       ptemp=ptemp->next;
		     }
		     printf("success=%d \n",success);
		     }
		     */

		   }
		   else {
		     temp1=F;
		     temp=F;
		     while (temp1){
		       temp=temp1;
		       temp1=temp1->next;
		     }
		     if (temp){
		       temp->next=LabelList[sink];
		       if(LabelList[sink])
			 LabelList[sink]->prev=temp;
		       LabelList[sink]=F;
		     }

		   }
		 }
		 //printf("Size of F is %d, size of the list for %d is %d \n",sizeofF,j,numLabels[j]);
		     
		 if (success){
		   E[j]=j;
		   // printf("vj=%d is added to E\n",j);
		 }
	       
	       } //if feasible extension
	     }//if(j!=vi)&& (j!=source))
	     // }//if no set threshold or numLabels<threshold
	 }//for all j
       
	 //mark the labels as processed or candidate 
	 temp=LabelList[vi];
	 while (temp) {
	   
	   //if (vi==sink){
	   //if (temp->labeldata.Cost+VFcost-v<-PRECISIONLIM)
	   //  temp->cond='C';
	   // else
	   //temp->cond='P';
	       //}
	       //else 
	     temp->cond='P';
	   
	   temp=temp->next;
	 }
	 
	 E[vi]=-1;
       } //(List[vi]
     } //if vi!=-1
     else
       empty=1;
  }//while not empty
  
  // /* *********************************************************************************************************
  
  //(void) time(&t2);
  
  // printf("Finding labels for sink= %ld  ",(int) t2-t1);

 

  // ***************************************************************************************************************/
  //time(&t1);
 
  //Instead of the following algorithm use the merge sort algorithm above:
  /*
  if (stepcount==5){
  printf("Before Second ESSPRC Before domination alg\n");
  temp=LabelList[sink];
  while (temp){
    printf("[%lf, %d, %lf, %d,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->labeldata.unreach);
    for (j=0;j<NCust;j++) {
      //if (temp->labeldata.nodeseq[j]>0)
      printf("%d:%d  ",j,temp->labeldata.nodeseq[j]);
    }
    printf(")]\n");
    temp=temp->next;
  }
  }
  */

  success=0;
  F=0;
  if (LabelList[sink]){
    success=dominate(&F, &LabelList[sink],maxnumlabels, set_threshold,0,0);
    //success=dominate(&F, &LabelList[sink],maxnumlabels, 0,0,0);
    // printf("success=%d \n",success);
    LabelList[sink]=F;
    
    if (set_threshold!=1) {
      F=SORTLIST(LabelList[sink]);
      LabelList[sink]=F;
    }
  
  }

  else 
    printf("LabelList[sink] is empty \n");

  ListBegin=0; 
  changed=0;
  if (LabelList[sink]){
    // ListBegin=LabelList[sink];
    
    temp=LabelList[sink];
    labelnumber=0;
    mintime=(double) TIMELIMIT+1;

    
    //    printf("Before Second ESSPRC \n");
    while (temp){
      
      //printf("[%lf, %d, %lf, %d,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->labeldata.unreach);
      /*
      for (j=0;j<NCust;j++) {
	//if (temp->labeldata.nodeseq[j]>0)
	printf("%d:%d  ",j,temp->labeldata.nodeseq[j]);
      }
      printf(")]\n");
      */
      //temp->labeldata.first_node=labelnumber;
      if (mintime>temp->labeldata.Time)
	mintime=temp->labeldata.Time;
      labelnumber++;
      temp=temp->next;
    }
    // exit(9);
    printf("number of total labels =%d \n",labelnumber);

    changed=ESPPRC_Pairing (LabelList[sink], labelnumber,mintime, &ListBegin,v,maxnumlabels, set_threshold,d,sink,RyanFoster) ;
    
    
    //printf("After Second ESSPRC \n");

    /*
    temp=ListBegin;
    while (temp){
      printf("[%lf, %d, %lf, %c,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->cond);
      for (j=0;j<NCust;j++) {
	//	if (temp->labeldata.nodeseq[j] >0)
	printf("%d:%d  ",j,temp->labeldata.nodeseq[j]);
      }
      printf(")]\n");
      //temp->labeldata.first_node=labelnumber;
      temp=temp->next;
    }
    */

  }


  // (void) time(&t2);
  
  // printf("Second Step ESPPRC= %ld  ",(int) t2-t1);


 
 


  //delete LabelLists
  for (i=0;i<NCust+2;i++){
    temp=LabelList[i];
    while (temp){
      LabelList[i]=temp->next;
      if (LabelList[i])
	LabelList[i]->prev=0;
      temp->next=0;
      free(temp->labeldata.nodeseq);
      temp->labeldata.nodeseq=0;
      free(temp);
      temp=LabelList[i];
    }
    LabelList[i]=0;
  }
  free(dem);  
  free(E);
  if (LabelList)
    free(LabelList);
  LabelList=0;

  *SinkLabels=ListBegin;

  return (changed);
}
