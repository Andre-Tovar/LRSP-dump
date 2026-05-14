//the function to eliminate the dominated labels:

int dominate (LABEL_NODE **list1,LABEL_NODE **list2,int maxlabelsize, int deletelabels, LABEL_NODE *Ptr, int pointer){
 

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
