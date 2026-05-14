#include "header.h"

int INFINITY;
#define INFINITY 100000;

typedef struct SavingList{
  int n1;
  int n2;
  double saving;
  struct SavingList *next;
} SAVINGLIST;


typedef struct Route {
  int left;
  int right;
  int size;
  int *seq;
  int cap;
  double time;
  double cost;
  struct Route *next;
  struct Route *prev;
} ROUTE;


SAVINGLIST *listsort(SAVINGLIST *list);
