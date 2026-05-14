#include "header.h"

double compare(LABEL_NODE *a, LABEL_NODE *b) {
    return (a->labeldata.Cost - b->labeldata.Cost);
} // compare()

int Generate1RandomNum (int begin, int end){

return((int)(rand()%(end - begin + 1))+ begin);

}  // Generate1RandomNum()

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
}  // SORTLIST()

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
  
}  //  reduceListWithCondN()


//the function to eliminate the dominated labels:

int dominate (int NCust, LABEL_NODE **list1,LABEL_NODE **list2,int maxlabelsize, int deletelabels, LABEL_NODE *Ptr, int pointer){
 

  LABEL_NODE *temp1,*temp2,*a1,*a2; 
  int passed,allsmall,i,updated,changed,x1,x2;
  LABEL_NODE *zel_n,*zel_p,*last_node;
 
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
} // dominate()


int ESPRC (int NCust, int TIMELIMIT, int VCap, double VFcost, double **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold, int RyanFoster)

{
  int i,j,k,vi;
  int *E;
  int *dem;
  int changed,empty,success,enter;
  
  LABEL_NODE **LabelList,*F,*temp,*temp1, *ListBegin; //*temp_next; 
  
  int sizeofF, labelnumber;
  double mintime;
  double add_dist;
  
  LabelList = (LABEL_NODE **) calloc (NCust+2, sizeof(LABEL_NODE *)); 

  dem=(int *) calloc (NCust+2, sizeof (int));
  for (i=0;i<NCust;i++)
    dem[i]=Demand[i];
  dem[source]=0;
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
 
  
  for (i=0;i<NCust+2;i++) {
    if (i!=source) 
      LabelList[i]=0;
  
  } // for i
  
  //node set..   
  E=(int *) calloc (NCust+2, sizeof(int));
  E[source]=source;  //if E[i]=i, i th node is in the list, E, otherwise it is -1:
  E[sink]=-1;
  for (i=0;i<NCust;i++) 
    E[i]=-1;

  empty=0; //shows the list of E is empty or not?
  
  while (!empty){    //&& (NumSoln<500)){  //IF THE SET IS NOT EMPTY:
     vi=-1;

     j=Generate1RandomNum(0,NCust+1);
     
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
	 } // if E[i] == i
       } // for i
     } // if vi == -1

     if (vi!=-1) {
       if  (LabelList[vi]){ //if set E is not empty
	 
	 for (j=0;j<NCust+2;j++) {
	 
	    if ((j!=vi)&& (j!=source)){
	       if (RyanFoster==0)
		 add_dist=d[vi][j]+d[j][sink];
	       else
		 add_dist=d[vi][j];

	       if ((add_dist<=TIMELIMIT)&&(dem[vi]+dem[j]<=VCap)){
		 //   F = (LABEL_NODE *) calloc (1, sizeof(LABEL_NODE));  
		 F=0;
		 sizeofF=0;
		 temp=LabelList[vi];
		 
		 while (temp) { //for all labels of vi:

		   if (temp->cond=='N'){
		     enter=0;
		     if (j==sink) {
		       if ((VFcost-v>=0)&&(temp->labeldata.Cost+rc[vi][j]>-PRECISIONLIM))
			 enter=0;
		       else
			 enter=1;
		     } // if j == sink
		     else {
		       if (temp->labeldata.nodeseq[j]==0)
			 enter=1;
		     } // else

		     if (enter){
		       if (RyanFoster==0)
			 add_dist=d[vi][j]+d[j][sink];
		       else
			 add_dist=d[vi][j];
		       if ((temp->labeldata.Time+add_dist<=TIMELIMIT)&&(temp->labeldata.Cap+dem[j]<=VCap)) { //if a feasible extension
		      
			 // fprintf(stderr,"A label is created for node %d\n",j);
			 temp1=(LABEL_NODE *) calloc(1,sizeof(LABEL_NODE));
			 
			 temp1->labeldata.Time=temp->labeldata.Time+d[vi][j]; 
		
			 if (j==sink)
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
			   } // if k == j
			   else if ((temp->labeldata.nodeseq[k]>0) || (temp->labeldata.nodeseq[k]==-1 )){
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			   } // else
			   else if ((j==sink) && (temp->labeldata.nodeseq[k]==-2 )){
			     if ((RyanFoster==0)&&(temp1->labeldata.Time+d[k][sink]+d[j][k]>TIMELIMIT))
			       temp1->labeldata.nodeseq[k]=-1;
			     else {
			       temp1->labeldata.nodeseq[k]=0;
			       temp1->labeldata.unreach--;
			     } // else
			   } // else
			   else if ((j!=sink) && (temp->labeldata.nodeseq[k]==-2 ))
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];

			   else if ((RyanFoster==0)&&(temp1->labeldata.Time+d[k][sink]+d[j][k]>TIMELIMIT)) { //||(temp1->labeldata.Cap+dem[k]>VCap)) {
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			     temp1->labeldata.nodeseq[k]=-1; //unreachable BECAUSE OF TIMELIMIT
			   } // else
			   else if ((j!=sink)&&(temp1->labeldata.Cap+dem[k]>VCap)){
			     temp1->labeldata.unreach=temp1->labeldata.unreach+1;
			     temp1->labeldata.nodeseq[k]=-2; //unreachable BECAUSE OF VEHICLE CAPACITY
			   }  // else
			   
			   else
			     temp1->labeldata.nodeseq[k]=temp->labeldata.nodeseq[k];
			 } // for k
			 
			 //For sink node, delete if number of unreachable >= NCust
		
			 
			 if ((j==sink)&&(temp1->labeldata.Cost>-PRECISIONLIM)&&(VFcost-v>=0)){

			     free(temp1->labeldata.nodeseq);
			     free(temp1);
			     temp1=0;
			 } // if j == sink ...
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
			 } // else
		       } //if feasible extension
		     }//if (j==sink)||(temp->labeldata.nodeseq[j]==0)
		   }//if cond==N
		   temp=temp->next;
		 }//while temp
	       
		 //look whether labels of j is changed or not  
	       
		 success=0;
		 
		 if (F){
		   if (j!=sink){
		     success=dominate(NCust, &LabelList[j],&F,maxnumlabels, set_threshold,0,0);   
		   } // if
		   else {
		     temp1=F;
		     temp=F;
		     while (temp1){
		       temp=temp1;
		       temp1=temp1->next;
		     } // while 
		     if (temp){
		       temp->next=LabelList[sink];
		       if(LabelList[sink])
			 LabelList[sink]->prev=temp;
		       LabelList[sink]=F;
		     } // if temp

		   } // else
		 } // if (F)
		 //printf("Size of F is %d, size of the list for %d is %d \n",sizeofF,j,numLabels[j]);
		     
		 if (success){
		   E[j]=j;
		   // printf("vj=%d is added to E\n",j);
		 } // if 
	       
	       } //if feasible extension
	     }//if(j!=vi)&& (j!=source))
	     // }//if no set threshold or numLabels<threshold
	 }//for all j
       
	 //mark the labels as processed or candidate 
	 temp=LabelList[vi];
	 while (temp) {
	    temp->cond='P';
	    temp=temp->next;
	 } // while
	 
	 E[vi]=-1;
       } //(List[vi]
     } //if vi!=-1
     else
       empty=1;
  }//while not empty
  
  success=0;
  F=0;
  if (LabelList[sink]){
    success=dominate(NCust, &F, &LabelList[sink],maxnumlabels, set_threshold,0,0);
    LabelList[sink]=F;
    
    if (set_threshold!=1) {
      F=SORTLIST(LabelList[sink]);
      LabelList[sink]=F;
    } // if set_threshold != 1
  
  } // if LabelList[sink]

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
      
      if (mintime>temp->labeldata.Time)
	mintime=temp->labeldata.Time;
      labelnumber++;
      temp=temp->next;
    } // while (temp)
    
    printf("number of total labels =%d \n",labelnumber);

// next line commented out by RTB 11/14/2011 -- do we need this for 1 step exact??
//    changed=ESPPRC_Pairing (LabelList[sink], labelnumber,mintime, &ListBegin,v,maxnumlabels, set_threshold,d,sink,RyanFoster) ;
        
    //printf("After Second ESSPRC \n");

  } // if LabelList


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
    } // while temp
    LabelList[i]=0;
  } // for i
  free(dem);  
  free(E);
  if (LabelList)
    free(LabelList);
  LabelList=0;

  *SinkLabels=ListBegin;

  return (changed);
} // ESPRC()
