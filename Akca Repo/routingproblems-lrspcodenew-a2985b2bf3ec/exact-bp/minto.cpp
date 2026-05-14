/*
 *     MINTO - Mixed INTeger Optimizer
 *
 *     VERSION 3.0
 *
 *     Author:    M.W.P. Savelsbergh
 *                School of Industrial and Systems Engineering
 *                Georgia Institute of Technology
 *                Atlanta, GA 30332-0205
 *                U.S.A.
 *
 *     (C)opyright 1992-1998 - M.W.P. Savelsbergh
 */

/*
 * MAIN.C
 */

extern "C"{
#include "header.h"
#include "minto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

int NFac;      // number of facilities
int NCust;     // number of customers
double *FCost;   // facility fixed costs
double VFcost;   //vehicle fix cost
double VOcost;  //vehicle operating cost
int VCap; //Vehicle capacity
int FCap; //Facility capacity
double **dist;
int *PairNum;
int *Demand;
int source;
int sink;
int counter;
int TIMELIMIT;
char CurrMpsF[BUFSIZE];
int newnode;

char *xstat;
char *xstat_l;
char *xstat_g;
int *x_rhs_l;
int *x_rhs_g;

NDINFO *nodeinfo[MAXNODES];
//int num_created;

int stepcount;
int numRows;
ColumnInfo **Col_Array;  

BranchDec *Rules;
double heur_upper_bnd;
int setupperbnd;
int CandFAC;
int CandCUST;
int CandN1;
int CandN2;
int ApplyRule4;
CONSTRAINTS *ConList; //constraint list
int *DoCWHeurColumnGen;
int *max_num_labels; 
int *SKIPFAC;
int var_index;
int numsubsetpri;
int MYUB;

int solve_pri_foronefac;
int chosenFAC_ind;

clock_t t1,t2; 

/*
#ifdef PROTOTYPING
int main (int, char **);
extern void minto (char *, char *);
#else
int main ();
extern void minto ();
#endif
*/

/*
 * main ()
 */

int
main (int argc, char **argv)
{
    int i;
    char buf[BUFSIZ], *c;
    fprintf(stderr,"inside minto.c\n");
    if (argc < 2) {
        fprintf (stderr, "USAGE: %s [-xo{.}m{.}t{.}be{.}E{.}p{.}hcikgfrRB{.}sn{1,2,3}a] filename (NO EXTENSION)\n", argv[0]);
		exit (1);
    }
    
    /*
     * Call MINTO
     */

    if (argc == 2) {
        minto (argv[argc-1], NULL);
    }
    else {
        c = buf;
        for (i = 1; i < argc-1; i++) {
            strcpy (c, argv[i]);
            c += strlen (argv[i]);
            *c++ = (i != argc-2 ? ' ' : '\0');
        }
        minto (argv[argc-1], buf);
    }
    return 0;
}
