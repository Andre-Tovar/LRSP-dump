
#include "header.h"

double compare(LABEL_NODE *a, LABEL_NODE *b) {
    return (a->labeldata.Cost - b->labeldata.Cost);
}


LABEL_NODE *reduceList(LABEL_NODE *list, int maxnum) {

  LABEL_NODE *temp,*temp2;
  int count;

  // printf("In reduceList\n");

  temp=list;

  count=0;

  while (temp){
    //  printf("count=%d \n",count);
    count++;
    temp=temp->next;
    if (count>=maxnum)
      break;
  }
  //printf("count=%d \n",count);

  if (temp){
    //printf("There are labels to be deleted\n");
    (temp->prev)->next=0;
    temp->prev=0;
  }

  //delete the rest starting from label temp
  
  while (temp) {
    //printf("Delete some of the labels\n");
    temp2=temp->next;
    if (temp2)      
      temp2->prev=0;
    temp->prev=0;
    temp->next=0;
    
    free(temp->labeldata.nodeseq);
    free(temp);
    temp=temp2;
  } //while

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

int dominate (LABEL_NODE **list1,LABEL_NODE **list2,int maxlabelsize, int deletelabels){
 

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
      x1=((a1->labeldata.Time<=a2->labeldata.Time)&& (a1->labeldata.Cost<=a2->labeldata.Cost)&& (a1->labeldata.unreach<=a2->labeldata.unreach)&&(a1->labeldata.Cap<=a2->labeldata.Cap));
      x2=((a1->labeldata.Time>=a2->labeldata.Time)&& (a1->labeldata.Cost>=a2->labeldata.Cost)&& (a1->labeldata.unreach>=a2->labeldata.unreach)&&(a1->labeldata.Cap>=a2->labeldata.Cap));
      if (x1 || x2 ){
	if (x1){
	  // printf("x1 is selected\n");
	  // printf("allsmall");
	  allsmall=1;
	  //printf("allsmall");
	  //printf("NCust=%d", NCust);
	  for (i=0;i<NCust;i++) {
	    // printf("%d ",i);
	    if (((a1->labeldata.nodeseq[i]>0)||(a1->labeldata.nodeseq[i]==-1))&&(a2->labeldata.nodeseq[i]==0)){
	      allsmall=0;
	      break;
	    }
	  }//for
	  if (allsmall){
	    // printf("all small \n");
	    //delete a2;
              zel_p=a2->prev;
	      zel_n=a2->next;
	      a2->prev=0;
	      a2->next=0;
	    
	      free(a2->labeldata.nodeseq);
	      free(a2); 
	      
	      a2=zel_n;
	      if (zel_p)
		zel_p->next=a2;
	      if (a2)
		a2->prev=zel_p;
	  }//if allsmall, deleting a2
	  else
	    a2=a2->next;
	}//if x1
	
	else if (x2){
	 
	  allsmall=1;
	  for (i=0;i<NCust;i++) {
	    if (((a2->labeldata.nodeseq[i]>0)||(a2->labeldata.nodeseq[i]==-1))&&(a1->labeldata.nodeseq[i]==0)){
	      allsmall=0;
	      break;
	    }
	  }//for
	  if (allsmall){
	    //delete a1:
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
	    free(a1); 
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
   limitedList=reduceList(sortedlist,maxlabelsize);
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


int ESPRC (int **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold)

{
  int i,j,k,vi;
  int *E;
  int *dem;
  int changed,empty,success,enter;
  double min;
  LABEL_NODE **LabelList,*F,*temp,*temp1,*ListEnd, *ListBegin, *marked; //*temp_next; 
  // int *numLabels;
  int sizeofF;

  //  numLabels=(int *)calloc (NCust+2,sizeof(int));


  LabelList = (LABEL_NODE **) calloc (NCust+2, sizeof(LABEL_NODE *)); 

  dem=(int *) calloc (NCust+2, sizeof (int));
  for (i=0;i<NCust;i++)
    dem[i]=Demand[i];
  dem[source]=0;
  dem[sink]=-VCap;

  //intitialization:
  LabelList[source]=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
  
  LabelList[source]->labeldata.Cap=0;
  LabelList[source]->labeldata.Time=0;
  LabelList[source]->labeldata.Cost=0.0;
  LabelList[source]->labeldata.nodeseq= (int *) calloc (NCust,sizeof(int));
  LabelList[source]->labeldata.count=0;
  LabelList[source]->labeldata.unreach=0;
  LabelList[source]->next=0; 
  LabelList[source]->prev=0; 
  LabelList[source]->cond='N';//new label
 
  
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
 
  while (!empty){    //&& (NumSoln<500)){  //IF THE SET IS NOT EMPTY:
     vi=-1;

     j=Generate1RandomNum(0,NCust+2);

     for (i=j;i<NCust+2;i++) {
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
	 // printf("vi=%d \n",vi);
	 for (j=0;j<NCust+2;j++) {
	   //if ((set_threshold==0)||(numLabels[j]<threshold)) {
	     if ((j!=vi)&& (j!=source)){
	       if ((d[vi][j]+d[j][sink]<=TIMELIMIT)&&(dem[vi]+dem[j]<=VCap)){
		 //   F = (LABEL_NODE *) calloc (1, sizeof(LABEL_NODE));  
		 F=0;
		 sizeofF=0;
		 // fprintf(stderr,"-------vj=%d----------------------------\n",j);
		 temp=LabelList[vi];
		 while (temp) { //for all labels of vi:
		   
		   if (temp->cond=='N'){
		     // if ((j==sink)||(temp->labeldata.nodeseq[j]==0)){ //be careful segmentation error might be nodeseq does not have sink  //if not in path
		     enter=0;
		     if (j==sink)
		       enter=1;
		     else if (temp->labeldata.nodeseq[j]==0)
		       enter=1;
		     if (enter){
		       if ((temp->labeldata.Time+d[vi][j]+d[j][sink]<=TIMELIMIT)&&(temp->labeldata.Cap+dem[j]<=VCap)) { //if a feasible extension
		      
			 // fprintf(stderr,"A label is created for node %d\n",j);
			 temp1=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
			 
			 temp1->labeldata.Time=temp->labeldata.Time+d[vi][j]; 
			 if (temp->labeldata.Cap+dem[j]<0)
			   temp1->labeldata.Cap=0;
			 else
			   temp1->labeldata.Cap=temp->labeldata.Cap+dem[j];
			 
			 temp1->labeldata.Cost=temp->labeldata.Cost+rc[vi][j];
			 temp1->labeldata.nodeseq=(int *) calloc (NCust, sizeof(int));
			 temp1->labeldata.count=temp->labeldata.count+1;
			 temp1->labeldata.unreach=temp->labeldata.unreach;
			 for (k=0;k<NCust;k++) {
			   if (k==j) {
			     temp1->labeldata.nodeseq[k]=temp1->labeldata.count;
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			   }
			   else if ((temp->labeldata.nodeseq[k]>0) || (temp->labeldata.nodeseq[k]==-1 )){
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			   }
			   else if ((temp1->labeldata.Time+d[k][sink]+d[j][k]>TIMELIMIT)) { //||(temp1->labeldata.Cap+dem[k]>VCap)) {
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			     temp1->labeldata.nodeseq[k]=-1; //unreachable
			   }
			   else
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			 }
			 
			 temp1->cond='N';
			 temp1->prev=0; //add label to F
			 temp1->next=F;
			 if (F)		 
			   F->prev=temp1;
			 F=temp1;
		       
			 sizeofF++;

		       } //if feasible extension
		     }//if (j==sink)||(temp->labeldata.nodeseq[j]==0)
		   }//if cond==N
		   temp=temp->next;
		 }//while temp
	       
		 //look whether labels of j is changed or not  
	       
		 success=0;
		 
		 if (F)

		   success=dominate(&LabelList[j],&F,maxnumlabels, set_threshold);
		 
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
	   if (i==sink){
	     if (temp->labeldata.Cost+VFcost-v<-PRECISIONLIM)
	       temp->cond='C';
	     else
	       temp->cond='P';
	   }
	   else
	     temp->cond='P';
	   temp=temp->next;
	 }
	 
	 E[vi]=-1;
       } //(List[vi]
     } //if vi!=-1
     else
       empty=1;
  }//while not empty
  
 /* *********************************************************************************************************
  printf("labels:[cost,demand,Time,cond,(path)]\n");
  for (i=0;i<NCust+2;i++){
    temp=LabelList[i];
    printf("node %d\n",i);
    while (temp){
      printf("[%lf, %d, %d, %c,(",temp->labeldata.Cost,temp->labeldata.Cap,temp->labeldata.Time,temp->cond);
      for (j=0;j<NCust;j++)
	printf("%d  ",temp->labeldata.nodeseq[j]);
      printf(")]\n");
      temp=temp->next;
    }
  }
 ***************************************************************************************************************/

  //  min=-PRECISIONLIM; 
  //To test the sorting ///
  /* printf("list the reduced cost of labels in sink\n");
  temp=LabelList[sink];
  j=0;
  while (temp){
    printf("%d --  %lf \n",j, temp->labeldata.Cost+VFcost-v);
    j++;
    temp=temp->next;
  } //while
  printf("*****************end of list the reduced cost of labels in sink\n");
  */
 
  //Instead of the following algorithm use the merge sort algorithm above:
  
  changed=0;
  ListBegin=0; 
  ListBegin=SORTLIST(LabelList[sink]);
  if (ListBegin->cond=='C')
    changed=1;



  //SORTING THE LABEL LIST OF THE SINK
  //First find the minimum, delete the label and put it to another sorted list.

  /*
  
  changed=1;
  ListEnd=0;
  ListBegin=0;

  while (changed){
    temp=LabelList[sink];
    min=MY_INF;
    changed=0;
    while(temp){
      if (temp->cond=='C') {
	if (temp->labeldata.Cost<min) {
	  min=temp->labeldata.Cost;
	  marked=temp;
	  if (!changed)
	    changed=1;
	} //if
      }//if cond=C
      temp=temp->next;
    } //while temp
    if (changed) {
      if (marked->prev){
	(marked->prev)->next=marked->next;
	if (marked->next)
	  (marked->next)->prev=marked->prev;
      } //if marked->prev
      else {
	LabelList[sink]=marked->next;
	if (marked->next)
	  (marked->next)->prev=0;
      }
      marked->prev=0;
      marked->next=0;
      if (ListBegin) {
	ListEnd->next=marked;
	marked->prev=ListEnd;
	ListEnd=marked;
      }//if listbegin
      else {
	ListEnd=marked;
	ListBegin=marked;
      }
      marked=0;
    }//changed
  }//while not changed
  
  */


  //to test the sorting
  /* printf("list the reduced cost of the labels in sorted list\n");
  temp=ListBegin;
  j=0;
  while (temp){
    printf("%d --  %lf \n",j, temp->labeldata.Cost+VFcost-v);
    j++;
    temp=temp->next;
  } //whilev[FAC]
  printf("#####################################################\n");
  

  printf("list the reduced cost of the labels in remaining sink labels\n");
  temp=LabelList[sink];
  j=0;
  while (temp){
    printf("%d --  %lf \n",j, temp->labeldata.Cost+VFcost-v);
    j++;
    temp=temp->next;
  } //while
  printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");

  exit(1);
  */

 //////////////////////////////////////////////////////////
  //delete LabelLists
  for (i=0;i<NCust+1;i++){
    temp=LabelList[i];
    while (temp){
      LabelList[i]=temp->next;
      if (LabelList[i])
	LabelList[i]->prev=0;
      temp->next=0;
      free(temp->labeldata.nodeseq);
      free(temp);
      temp=LabelList[i];
    }
  }
  free(dem);  
  free(E);
  free(LabelList);

  /*
  if (ListBegin)
    changed=1;
  else
    changed=0;
  */

  *SinkLabels=ListBegin;

  return (changed);
}
