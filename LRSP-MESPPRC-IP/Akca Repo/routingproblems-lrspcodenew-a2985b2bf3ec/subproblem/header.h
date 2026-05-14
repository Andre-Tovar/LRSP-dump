#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define BUFSIZE 64
#define MAXNUMLABELS 10000
#define MyEPS 0.000001
#define PRECISIONLIM 0.0001
//#define NAME_SIZE 16
#define MY_INF 100000


typedef struct Label{
  int Cap;
  double Time;
  double Cost;
  int *nodeseq; //[NCust]
  int count;
  int unreach;
  //int first_node;
  int first_node; //for solve_pri_foronefac=1;
  int last_node; //for solve_pri_foronefac=1;
} LABEL;


typedef struct Label2{
  LABEL labeldata;
  struct Label2 *next;
  struct Label2 *prev;
  char cond;
} LABEL_NODE;

//extern int ESPRC(double **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold,int RyanFoster);
extern int ESPRC(int NCust, int TIMELIMIT, int VCap, double VFCost, double **d, double **rc,int *Demand,int source, int sink, LABEL_NODE **SinkLabels, double v, int maxnumlabels, int set_threshold, int RyanFoster);


#endif
