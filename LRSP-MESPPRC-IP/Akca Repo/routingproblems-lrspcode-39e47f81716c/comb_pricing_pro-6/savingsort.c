
#include "header_cw.h"

int cmp(SAVINGLIST *a, SAVINGLIST *b) {
    return b->saving - a->saving;
}


SAVINGLIST *listsort(SAVINGLIST *list) {
  SAVINGLIST *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;

    /*
     * Silly special case: if `list' was passed in as NULL, return
     * NULL immediately.
     */
  if (!list)
    return NULL;

  insize = 1;

  while (1) {
    p = list;
    ///oldhead = list;		       /* only used for circular linkage */
    list = NULL;
    tail = NULL;

    nmerges = 0;  /* count number of merges we do in this pass */
    
    while (p) {
      nmerges++;  /* there exists a merge to be done */
      /* step `insize' places along from p  nnn */
      q = p;
      psize = 0;
      for (i = 0; i < insize; i++) {
	psize++;
	//if (is_circular)
	//    q = (q->next == oldhead ? NULL : q->next);
	//else
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
	  //if (is_circular && q == oldhead) q = NULL;
	} else if (qsize == 0 || !q) {
	  /* q is empty; e must come from p. */
	  e = p; p = p->next; psize--;
	  //    if (is_circular && p == oldhead) p = NULL;
	} else if (cmp(p,q) <= 0) {
	  /* First element of p is lower (or same);
	   * e must come from p. */
	  e = p; p = p->next; psize--;
	  //if (is_circular && p == oldhead) p = NULL;
	} else {
	  /* First element of q is lower; e must come from q. */
	  e = q; q = q->next; qsize--;
	  //if (is_circular && q == oldhead) q = NULL;
	}

	/* add the next element to the merged list */
	if (tail) {
	  tail->next = e;
	} else {
	  list = e;
	}
	//if (is_double) {
		    /* Maintain reverse pointers in a doubly linked list. */
	//    e->prev = tail;
	//}
	tail = e;
      }

      /* now p has stepped `insize' places along, and q has too */
      p = q;
    }
    /*if (is_circular) {
        tail->next = list;
	    if (is_double)
		list->prev = tail;
		} else*/
    tail->next = NULL;

    /* If we have done only one merge, we're finished. */
    if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
      return list;
    
    /* Otherwise repeat, merging lists twice the size */
    insize *= 2;
  }
}
