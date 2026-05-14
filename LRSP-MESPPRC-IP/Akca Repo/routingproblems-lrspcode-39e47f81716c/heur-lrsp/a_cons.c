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
 *     (C)opyright 1992-1994 - M.W.P. Savelsbergh
 */

/*
 * A_CONS.C
 */


#include "header.h"
/*
 * appl_constraints
 */
 
unsigned
appl_constraints (id, zlp, xlp, zprimal, xprimal,
    nzcnt, ccnt, cfirst, cind, ccoef, csense, crhs, ctype, cname, sdim, ldim)
int id;               /* identification of active minto */
double zlp;           /* value of the LP solution */
double *xlp;          /* values of the variables */
double zprimal;       /* value of the primal solution */
double *xprimal;      /* values of the variables */
int *nzcnt;           /* variable for number of nonzero coefficients */
int *ccnt;            /* variable for number of constraints */
int *cfirst;          /* array for positions of first nonzero coefficients */
int *cind;            /* array for indices of nonzero coefficients */
double *ccoef;        /* array for values of nonzero coefficients */
char *csense;         /* array for senses */
double *crhs;         /* array for right hand sides */
int *ctype;           /* array for the constraint types: LOCAL or GLOBAL */
char **cname;         /* array for names */
int sdim;             /* length of small arrays */
int ldim;             /* length of large arrays */
{

  CONSTRAINTS *addcon, *addcon1;
  int i,j,d,fac,pairNO;
  char *s;
  // char ConName[BUFSIZE];
  int size;
  char *_cstore;
  int numberofCONST, numofNEWCON;

  // FILE *InfoCOL;
  //char Col_file[BUFSIZE];
  //ColumnInfo *column;
  
  if (id) {// if recursive version of minto, no constraints are added
    return (FAILURE);
  }

  if (counter == 1 || newnode == FALSE){
   
    return (FAILURE);
  }


  if (newnode == TRUE)
    newnode = FALSE;

 

   
  *ccnt=0;
  *nzcnt=0;

  
  
  if (!ConList)
    return(FAILURE);

  inq_form();
  
  numberofCONST=info_form.form_ccnt;
  numofNEWCON=0;
  for (i=numRows;i<numberofCONST;i++){
    inq_constr(i);
    if (info_constr.constr_status!=DELETED){
      numofNEWCON++;
      printf("constraint with index %d is NOT DELETED \n",i);

    }
    else
      printf("constraint with index %d is DELETED \n",i);
  } //for i
 
  if (numofNEWCON!=0){

    printf("numberof constraints in current model=%d, number of rows in original=%d, number of non-deleted=%d \n ",numberofCONST,numRows,numofNEWCON);
    printf("ERROR, the previous constraints are not deleted \n");
    exit(1);
  }
    
  //add ALL constraints from ConList
  
  addcon=ConList;
  
  // size= NAME_SIZE*nnewcon;
  //printf("nnewcon=%d \n",nnewcon);

  size= NAME_SIZE;
  _cstore=0;
  //_cstore = (char *) calloc(size, sizeof(char));

  while (addcon) {
    
    printf("CandFAC=%d, CandCUST=%d \n", addcon->facility,addcon->customer);
    
    cfirst[*ccnt]=*nzcnt;
    
    if (_cstore)
      _cstore=0;
    _cstore = (char *) calloc(size, sizeof(char));
    
    for (j=2*NFac+NCust+1; j <info_form.form_vcnt; j++) {
      
      inq_var(j,YES);
      
      //INSTEAD I CAN CHECK FOR NONNEGATIVE COEFFICINET IN aijpZjp<Tj constraint
      
      s= (char*) calloc (NAME_SIZE,sizeof(char)); 
      strcpy(s,info_var.var_name);
      d=0;   
      d=getPairNum(s,&fac,&pairNO); 
      free(s);
      d=0;
      
      if (fac==addcon->facility){
	for (i=0; i <info_var.var_nz; i++) {
	  if (info_var.var_ind[i]==addcon->customer){
	    //printf("pairNO=%d \n",pairNO);
	    cind[*nzcnt]=j;
	    ccoef[(*nzcnt)++]=1.0;
	    break;
	  } //if had a non zero coeff
	} //for i
      } //if facility=
    } //for all j
    

    if (addcon->type==LESSCON){
      crhs[*ccnt] = (double) 0;
      csense[*ccnt] = 'L';
      /*
      printf("A constraint is added: Customer %d CANNOT be assigned to facility %d \n", addcon->customer, addcon->facility);
      sprintf(&(_cstore[NAME_SIZE*(*ccnt)]),"F%d_C%d_F%d",0,addcon->customer, addcon->facility);
      _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
      cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
      */

      printf("A constraint is added: Customer %d CANNOT be assigned to facility %d \n", addcon->customer, addcon->facility);
      sprintf(&(_cstore[0]),"F%d_C%d_F%d",0,addcon->customer, addcon->facility);
      _cstore[size - 1] = '\0';
      cname[*ccnt] = _cstore;

      //sprintf(ConName, "Forbid_C%d_F%d",addcon->customer, addcon->facility);
      //cname[*ccnt] = ConName;
      
    }
    
    else if (addcon->type==GREATERCON){
      crhs[*ccnt] = (double) 1;
      csense[*ccnt] = 'G';
      /*
      printf("A constraint is added: Customer %d MUST be assigned to facility %d \n", addcon->customer, addcon->facility);
      sprintf(&(_cstore[NAME_SIZE*(*ccnt)]),"A%d_C%d_F%d",1,addcon->customer, addcon->facility);
      _cstore[NAME_SIZE*(*ccnt + 1) - 1] = '\0';
      cname[*ccnt] = &(_cstore[NAME_SIZE*(*ccnt)]);
      
      */
      printf("A constraint is added: Customer %d MUST be assigned to facility %d \n", addcon->customer, addcon->facility);
      sprintf(&(_cstore[0]),"A%d_C%d_F%d",1,addcon->customer, addcon->facility);
      _cstore[size- 1] = '\0';
      cname[*ccnt] = _cstore;
      

      // sprintf(ConName, "Assgn_C%d_F%d",addcon->customer, addcon->facility);
      //cname[*ccnt] = ConName;
    }
    
    
    ctype[(*ccnt)++]=LOCAL;
    
    addcon=addcon->next;
  } //while addcon
  
  cfirst[*ccnt]=*nzcnt;
  
  //  if (_cstore) 
  //free(_cstore);
  //_cstore=0;

  return (SUCCESS);

  
  
  /*
    
  sprintf(Col_file,"%s.col",inq_file());
  InfoCOL=fopen(Col_file,"w");
  fprintf(InfoCOL, "File name: %s\n",inq_file());
  fprintf(InfoCOL, "Column information:\n");
      
  for (i=0;i<NFac;i++){
    column=Col_Array[i];
    fprintf(InfoCOL, "\n **************FACILITY=%d *******************************************\n",i);
    while (column){
      fprintf(InfoCOL, "Y_%d_%d : ",i,column->Name);
      fprintf(InfoCOL, "%d: ", column->Col_Time);
      
      for (j=0;j<NCust;j++){
	if (column->sequence[j])
	  fprintf(InfoCOL, "%d:%d ,",j,column->sequence[j]);
      }//for j
      fprintf(InfoCOL, "\n");
      column=column->next;
    }
  } //for i
  
  fprintf(InfoCOL, "\n END *************************************\n");
  fclose(InfoCOL);
  
  */
  

  //  printf("number of constraints added=%d \n",*ccnt);
  //exit(9);
  
  // return(*ccnt > 0 ? SUCCESS:FAILURE);
 

 
  


}



