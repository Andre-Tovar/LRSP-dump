//$Id:$

#include <ilcplex/cplex.h>


extern "C" void InitLP(void);
extern "C"{
   
#include "minto.h"
#include "stdio.h"
#include "stdlib.h"
}
CPXENVptr env=NULL;
CPXLPptr lp=NULL;
//extern MIO mio;



void InitLP()
{

  int cplxrc = -1;

  env = CPXopenCPLEX (&cplxrc);
  if (cplxrc > 0) {
    fprintf (stderr, "MintO: Error in initializing CPLEX (%d)\n", cplxrc);
    exit (1);
  }
  lp = CPXcreateprob(env,&cplxrc,"MY_PROB");
  if ( lp == NULL ) {
      fprintf (stderr, "Failed to create LP.\n");
      exit(1);
  }
}
