/*  
 *     MINTO - Mixed INTeger Optimizer
 *
 *     VERSION 3.2
 *
 *     Author:    M.W.P. Savelsbergh
 *                School of Industrial and Systems Engineering
 *                Georgia Institute of Technology
 *                Atlanta, GA 30332-0205
 *                U.S.A.
 * 
 *                Jeff T. Linderoth
 *                School of Industrial and Systems Engineering
 *                Lehigh University
 *                Bethlehem, PA 18015
 *
 *     (C)opyright 1992-2006 - M.W.P. Savelsbergh, 
 *                             J. T. Linderoth
 *
 *  $Id: minto.h,v 1.2 2006-09-06 16:18:43 jeff Exp $  
 */


/*
 * CONSTANT DEFINITIONS
 */

/*
 * Boolean constants
 */

#define TRUE  1
#define FALSE 0

#define ON    1
#define OFF   0

/*
 * Tolerance for floating point computations
 */

#if defined(UNIX)
#define EPS           1.0E-6
#endif

#if defined(WINNT) || defined(DOS)
#define EPS           1.0E-6
#endif

/*
 * Inifinity 
 */

#if defined(UNIX)
#if defined(CPLEX) || defined(XPRESS)
#define INF            1.0E20
#endif
#if defined(OSL)
#define INF            1.0E31
#endif
#endif

#if defined(WINNT) || defined(DOS)
#if defined(CPLEX)
#define INF            1.0E20
#endif
#if defined(XPRESS)
#define INF            1.0E20
#endif
#endif

#ifndef INF
#define INF            1.0E31
#endif

/*
 * Function return values
 */

#define FAILURE       0
#define SUCCESS       1
#define INSUFFICIENT  2
#define REORDER       3
#define FATHOM_NODE   4

#define STOP          0
#define CONTINUE      1

#define NO            0
#define YES           1

#define DEACTIVATED  -1
#define ERROR        -2

/*
 * Exit status codes
 */

#define E_OPTIMAL       1
#define E_TIMELIMIT     2
#define E_NODELIMIT     3
#define E_QUIT          4
#define E_SEGV          5
#define E_ILL           6
#define E_FPE           7
#define E_INFEASIBLE    8

/*
 * INQUIRY FUNCTIONS
 */

/*
 * Definitions of constants
 */

#define UNDEFINED     -1

/*
 * Variable classes
 */

#define CONTINUOUS    'C'
#define INTEGER       'I'
#define BINARY        'B'

/*
 * Variable infomation
 */

#define ORIGINAL                 0

#define MODIFIED_BY_MINTO        1
#define MODIFIED_BY_BRANCHING    2
#define MODIFIED_BY_APPL         3

/*
 * Constraint classes
 */

#define MIXUB                 0x00001
#define MIXEQ                 0x00002
#define NOBINUB               0x00004
#define NOBINEQ               0x00008
#define ALLBINUB              0x00010
#define ALLBINEQ              0x00020
#define SUMVARUB              0x00040
#define SUMVAREQ              0x00080
#define VARUB                 0x00100
#define VAREQ                 0x00200
#define VARLB                 0x00400
#define BINSUMVARUB           0x00800
#define BINSUMVAREQ           0x01000
#define BINSUM1VARUB          0x02000
#define BINSUM1VAREQ          0x04000
#define BINSUM1UB             0x10000
#define BINSUM1EQ             0x20000

/*
 * Constraint information
 */

#define GENERATED_BY_MINTO       1
#define GENERATED_BY_BRANCHING   2
#define GENERATED_BY_APPL        3

/*
 * Constraint types
 */

#define ORIGINAL    0
#define LOCAL       1
#define GLOBAL      2

/*
 * Internal management
 */

#define ACTIVE      0
#define INACTIVE    1
#define DELETED     2

#define DELETE      2

/*
 * Basis
 */

#define ATLOWER                  0
#define BASIC                    1
#define ATUPPER                  2
#define NONBASIC                 3

#define CNONBASIC                0
#define CBASIC                   1

/*
 * LP-solver control parameters
 */

/*
 * Algorithm
 */

#define PRIMAL                         0
#define DUAL                           1

#if defined(CPLEX)
#define BARRIER                        2
#define BARRIERCROSS                   3
#define HYBNETWORKPRIMAL               4
#define HYBNETWORKDUAL                 5
#endif

/*
 * Pricing
 */

#if defined(CPLEX)
/*
 * Primal Simplex
 */
#define REDUCED_COST                  -1
#define REDUCED_COST_DEVEX             0
#define DEVEX                          1
#define STEEPEST_EDGE                  2
#define STEEPEST_EDGE_SLACK_NORMS      3
#define FULL                           4
/*
 * Dual Simplex
 */
#define AUTO                           0
#define STANDARD_DUAL                  1
#define STEEPEST_EDGE_SLACK            3
#define STEEPEST_EDGE_NORMS            4
#endif

/*
 * Parameters
 */

#if defined(CPLEX)
#define BEGINNING                      1
#define AUTOMATIC                      0

#define STANDARD                       0
#define AGGRESSIVE                     1

#define PRESOLVE                       1
#define NOPRESOLVE                     0
#endif

/*
 * TYPE DEFINITIONS
 */

typedef struct
{
  int vlb_var;			/* ind of associated 0-1 variable */
  double vlb_val;		/* value of associated bound */
}
VLB;

typedef struct
{
  int vub_var;			/* ind of associated 0-1 variable */
  double vub_val;		/* value of associated bound */
}
VUB;

typedef struct info_form
{
  int form_vcnt;		/* number of variables in the formulation */
  int form_ccnt;		/* number of constraints in the formulation */
}
INFO_FORM;

typedef struct info_var
{
  char *var_name;		/* variable name */
  char var_class;		/* class: CONT, int, or BIN */
  double var_obj;		/* objective function coefficient */
  int var_nz;			/* number of constraints with nonzero coefficients */
  int *var_ind;			/* indices of constraints with nonzero coefficients */
  double *var_coef;		/* actual coefficients */
  int var_status;		/* ACTIVE or INACTIVE */
  double var_lb;		/* lower bound */
  double var_ub;		/* upper bound */
  VLB *var_vlb;			/* associated variable lower bound */
  VUB *var_vub;			/* associated variable upper bound */
  int var_lb_info;		/* ORIGINAL, MODIFIED_BY_MintO,
				   MODIFIED_BY_BRANCHING, or MODIFIED_BY_APPL */
  int var_ub_info;		/* ORIGINAL, MODIFIED_BY_MintO,
				   MODIFIED_BY_BRANCHING, or MODIFIED_BY_APPL */
  int var_activeix;             /* Index of Variable in Active LP formulation */
}
INFO_VAR;

typedef struct
{
  int obj_nz;			/* number of variables with nonzero coefficients */
  int *obj_ind;			/* indices of variables with nonzero coefficients */
  double *obj_coef;		/* actual coefficients */
}
INFO_OBJ;

typedef struct info_constr
{
  char *constr_name;		/* constraint name */
  int constr_class;		/* classification: ... */
  int constr_nz;		/* number of variables with nonzero coefficients */
  int *constr_ind;		/* indices of variables with nonzero coefficients */
  double *constr_coef;		/* actual coefficients */
  char constr_sense;		/* sense */
  double constr_rhs;		/* right hand side */
  int constr_status;		/* ACTIVE or INACTIVE */
  int constr_type;		/* LOCAL or GLOBAL */
  int constr_info;		/* ORIGINAL, GENERATED_BY_MintO,
				   GENERATED_BY_BRANCHING, or GENERATED_BY_APPL */
  int constr_activeix;          /* Id in the active (LP) formulation */
}
INFO_CONSTR;

typedef struct info_base
{
  int *base_vstat;		/* status of the variables */
  int *base_cstat;		/* status of the constraints */
}
INFO_BASE;

typedef struct info_opt
{
  int opt_stat;			/* exit status */
  double opt_value;		/* value of optimal solution */
  int opt_nzcnt;		/* number of nonzero's in optimal solution */
  int *opt_ix;			/* indices of nonzero's in optimal solution */
  double *opt_val;		/* values of nonzero's in optimal solution */
}
INFO_OPT;

/*
 * externAL VARIABLES DECLARATIONS
 */

#if defined(CPLEX)
#if !defined(VERSION30)
extern struct cpxenv *mintoenv;
#endif
extern struct cpxlp *mintolp;
#endif

extern INFO_FORM info_form;
extern INFO_OBJ info_obj;
extern INFO_CONSTR info_constr;
extern INFO_VAR info_var;
extern INFO_BASE info_base;
extern INFO_OPT info_opt;

/*
 * FUNCTION PROTOTYPE OF MintO
 */

extern void minto (char *, char *);

/*
 * FUNCTION PROTOTYPES OF APPLICATION FUNCTIONS
 */

extern unsigned int appl_bounds (int,
			     double, double *, double, double *, int *, int *,
			     char *, double *, int);
extern unsigned int appl_constraints (int, double, double *, double, double *,
				  int *, int *, int *, int *, double *,
				  char *, double *, int *, char **, int, int);
extern unsigned int appl_delconstraints (int, int *, int *);
extern unsigned int appl_divide (int,
			     long, long, double, double *, double, double *,
			     int *, int *, int *, char *, double *,
			     int *, int *, int *, int *, double *, char *,
			     double *, char **, int, int, int);
extern unsigned int appl_exit (int, double, double *);
extern unsigned int appl_fathom (int, double, double);
extern unsigned int appl_feasible (int, double, double *);
extern unsigned int appl_init (int);
extern unsigned int appl_initlp (int, int *);
extern unsigned int appl_mps (int,
			  int *, int *, int *, double **, double **,
			  double **, char **, char **, double **, int **,
			  int **, double **, int *, char **, char ***, int *,
			  char **, char ***);
extern unsigned int appl_node (int, long, long, double, double *);
extern unsigned int appl_preprocessing (int);
extern unsigned int appl_primal (int, double, double *, int, double, double *,
			     double *, double *, int *);
extern unsigned int appl_quit (int, double, double *);
extern unsigned int appl_rank (int, long, long, double, double, double *);
extern unsigned int appl_terminatenode (int, double, double, double *);
extern unsigned int appl_terminatelp (int, double, double *, double *);
extern unsigned int appl_variables (int,
				double, double *, double, double *,
				int *, int *, char *, double *, double *,
				double *, int *, int *, double *, char **,
				int, int);

/*
 * FUNCTION PROTOTYPES OF OTHER FUNCTIONS
 */

extern char *inq_file (void);
extern char *inq_prob (void);
extern void inq_form (void);
extern void inq_obj (void);
extern void inq_constr (int);
extern int inq_made_best_move();
extern void inq_var (int, int);
extern void set_obj (void);
extern void set_constr (int);
extern void set_var (int);
extern int minto_cix (char *);
extern int minto_vix (char *);
extern int lp_vcnt (void);
extern int lp_ccnt (void);
extern double lp_slack (int);
extern double lp_pi (int);
extern double lp_rc (int);
extern void lp_base (void);
extern int lp_cix (char *);
extern int lp_vix (char *);
extern void ctrl_clique (unsigned);
extern void ctrl_implication (unsigned);
extern void ctrl_knapcov (unsigned);
extern void ctrl_gubcov (unsigned);
extern void ctrl_flowcov (unsigned);
extern void ctrl_output (unsigned);
extern void ctrl_lpmethod (int);
#if defined(CPLEX)
extern void ctrl_lppricing (int, int);
extern void ctrl_lppricelist (int);
extern void ctrl_lpperturbconst (double);
extern void ctrl_lppertrubmethod (int);
extern void ctrl_lprefactorfreq (int);
extern void ctrl_lpscalingmethod (int);
#endif
extern void bypass_lp (unsigned);
extern void bypass_fathom (unsigned);
extern void bypass_integral (unsigned);
extern void wrt_prob (char *);
extern double stat_avnds (void);
extern long stat_maxnds (void);
extern long stat_evnds (void);
extern long stat_depth (void);
extern int stat_lpcnt (void);
extern double stat_gap (void);
extern int stat_maxlprows (void);
extern int stat_maxlpcols (void);
extern int stat_cliquecnt (void);
extern int stat_implicationcnt (void);
extern int stat_knapcovcnt (void);
extern int stat_gubcovcnt (void);
extern int stat_sknapcovcnt (void);
extern int stat_flowcovcnt (void);
extern double stat_time (void);
extern int stat_numsols (void);

/* For debugging */
extern int check_cols(int vcnt, int ccnt, const double *vobj, 
                      const int *vfirst, const int *vind,
                      const double *vcoef, const double *vlb,
                      const double *vub);

/* For setting parameters */
extern int MIO_set_int_param(int which_param, int value);
extern int MIO_set_double_param(int which_param, double value);

enum MioParameterType {

// Integer parameters

  /** Branching strategy */
  MIO_BRANCH_STRATEGY = 1000, 

  /** One of static or dynamic -- You can fix the number of iterations
      yourself, or have MINTO figure it out for you...
  */
  MIO_BRANCH_ITER_STRATEGY,

  /** Maximum number of iterations to do in strong branching */
  MIO_BRANCH_MAXSB_ITER,
  /** Maximum total number of variables to consider for strong branching */
  MIO_BRANCH_MAXSB_VARS,
  /** Maximum number of iterations for which to initialize pseudocosts */
  MIO_BRANCH_PSEUDOINIT_ITER,
  /** Number of times to initialize pseudocosts until you "trust" them */
  MIO_BRANCH_PSEUDO_NUMTRUST,


  /** Node selection stuff */
  MIO_NODESELECT_STRATEGY,

  MIO_HEURISTIC_STRATEGY,
  MIO_MAXLP_DIVING,

  MIO_PREP_STRATEGY,
  MIO_PREPROC_FREQ,

  MIO_CLIQUECUT_STRATEGY,
  MIO_IMPLICATIONCUT_STRATEGY,
  MIO_FLOWCUT_STRATEGY,
  MIO_GUBCUT_STRATEGY,
  MIO_KNAPSACKCUT_STRATEGY,

  MIO_CUT_FREQUENCY,
  MIO_CUT_POOLSIZE,
  MIO_CUT_DELBND,
  MIO_CUT_DEACTFREQ,

  MIO_RCF_FREQ,

  MIO_RESTART_STRATEGY,
  MIO_RESTART_CNT,

  MIO_PRIMAL_PHASE,
  MIO_PRIMAL_FREQ,

  MIO_PRINTLEVEL,

  MIO_XFLAG,
  MIO_AFLAG,
  MIO_NFLAG,
  MIO_SELECTMAX,
  MIO_LFLAG,
  MIO_BOUND_IMPR_FLAG,
  MIO_ROW_MNGT_FLAG,
  MIO_FORCE_BRANCHING_FLAG,
  MIO_SWITCH_STRATEGY,
  MIO_MAX_TIME,
  MIO_BOUND_UPDATING,
  MIO_LAST_INT_PARAM,

  // Double parameters
  MIO_BRANCH_SMALLDEG_WEIGHT = 5000,
  MIO_BRANCH_BIGDEG_WEIGHT,

  /** Percentage of fractional variables on which to do simplex pivots */
  MIO_BRANCH_SBPERCENT,

  MIO_RESTART_FRAC,
  
  MIO_ZCUTOFF,
  MIO_LAST_DOUBLE_PARAM,

  
};

/* 
  Local Variables:
  mode: c
  eval: (c-set-style "gnu")
  eval: (setq indent-tabs-mode nil)
  End:
*/
