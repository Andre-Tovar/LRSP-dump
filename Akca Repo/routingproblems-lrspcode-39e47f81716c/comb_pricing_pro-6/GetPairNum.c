#include "header.h"

int getPairNum (char *s, int *facility, int *pairNO){
  //returns 1 if successful 
  //otherwise exits
  
  char *t;



  t=strtok(s,"_");
  t=strtok('\0',"_");
  if (t) 
    *facility=atoi(t);
  else {
    fprintf(stdout,"cannot read name properly (in GetPairNum.c)\n");
    exit(1);
  }
  t=strtok('\0',"_");
  if (t)
    *pairNO=atoi(t);
  else {
    fprintf(stdout,"cannot read number of pair\n");
    exit(1);
  }
  return(1);
}
