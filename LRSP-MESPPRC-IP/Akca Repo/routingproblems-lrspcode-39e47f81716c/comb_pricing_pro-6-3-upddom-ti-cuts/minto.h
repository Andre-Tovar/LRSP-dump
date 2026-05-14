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
 *     (C)opyright 1992-1997 - M.W.P. Savelsbergh
 */

/*
 * MINTO.H
 *
 * Last modified: 8/7/97
 *
 *      This file should be included in all sources of an application
 *      that uses MINTO, a Mixed INTeger Optimizer.
 *
 *      It contains the type definitions, the constant definitions, the
 *      information structure definitions, the external variable declarations,
 *      and function prototypes.
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

/*
 * Function return values
 */
 
#define FAILURE       0
#define SUCCESS       1
#define INSUFFICIENT  2
#define REORDER       3

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

typedef struct {
    int     vlb_var;       /* ind of associated 0-1 variable */
    double  vlb_val;       /* value of associated bound */
} VLB;

typedef struct {
    int     vub_var;       /* ind of associated 0-1 variable */
    double  vub_val;       /* value of associated bound */
} VUB;
  
typedef struct info_form {
    int     form_vcnt;        /* number of variables in the formulation */
    int     form_ccnt;        /* number of constraints in the formulation */
} INFO_FORM;

typedef struct info_var {
    char    *var_name;     /* variable name */
    char     var_class;    /* class: CONT, INT, or BIN */
    double   var_obj;      /* objective function coefficient */
    int      var_nz;       /* number of constraints with nonzero coefficients */
    int     *var_ind;      /* indices of constraints with nonzero coefficients */
    double  *var_coef;     /* actual coefficients */
    int      var_status;   /* ACTIVE or INACTIVE */
    double   var_lb;       /* lower bound */
    double   var_ub;       /* upper bound */
    VLB     *var_vlb;      /* associated variable lower bound */
    VUB     *var_vub;      /* associated variable upper bound */
    int      var_lb_info;  /* ORIGINAL, MODIFIED_BY_MINTO,
                              MODIFIED_BY_BRANCHING, or MODIFIED_BY_APPL */
    int      var_ub_info;  /* ORIGINAL, MODIFIED_BY_MINTO,
                              MODIFIED_BY_BRANCHING, or MODIFIED_BY_APPL */
} INFO_VAR;

typedef struct {
    int      obj_nz;       /* number of variables with nonzero coefficients */
    int     *obj_ind;      /* indices of variables with nonzero coefficients */
    double  *obj_coef;     /* actual coefficients */
} INFO_OBJ;

typedef struct info_constr {
    char    *constr_name;   /* constraint name */
    int      constr_class;  /* classification: ... */
    int      constr_nz;     /* number of variables with nonzero coefficients */
    int     *constr_ind;    /* indices of variables with nonzero coefficients */
    double  *constr_coef;   /* actual coefficients */
    char     constr_sense;  /* sense */
    double   constr_rhs;    /* right hand side */
    int      constr_status; /* ACTIVE or INACTIVE */
    int      constr_type;   /* LOCAL or GLOBAL */
    int      constr_info;   /* ORIGINAL, GENERATED_BY_MINTO,
                               GENERATED_BY_BRANCHING, or GENERATED_BY_APPL */
    int constr_activeix;          /* Id in the active (LP) formulation */
} INFO_CONSTR;

typedef struct info_base {
    int     *base_vstat;    /* status of the variables */
    int     *base_cstat;    /* status of the constraints */
} INFO_BASE;

typedef struct info_opt {
    int      opt_stat;      /* exit status */
    double   opt_value;     /* value of optimal solution */
    int      opt_nzcnt;     /* number of nonzero's in optimal solution */
    int     *opt_ix;        /* indices of nonzero's in optimal solution */
    double  *opt_val;       /* values of nonzero's in optimal solution */
} INFO_OPT;

/*
 * EXTERNAL VARIABLES DECLARATIONS
 */

#if defined(CPLEX)
#if !defined(VERSION30)
extern struct cpxenv *mintoenv;
#endif
extern struct cpxlp *mintolp;
#endif

extern INFO_FORM    info_form;
extern INFO_OBJ     info_obj;
extern INFO_CONSTR  info_constr;
extern INFO_VAR     info_var;
extern INFO_BASE    info_base;
extern INFO_OPT     info_opt;

/*
 * FUNCTION PROTOTYPE OF MINTO
 */

#if defined(PROTOTYPING)
extern void minto (char *, char *);
#else
extern void minto ();
#endif

/*
 * FUNCTION PROTOTYPES OF APPLICATION FUNCTIONS
 */

#if defined(PROTOTYPING)
extern unsigned appl_bounds (int,
    double, double *, double, double *, int *, int *, char *, double *, int);
extern unsigned appl_constraints (int,
    double, double *, double, double *,
    int *, int *, int *, int *, double *, char *,
    double *, int *, char **, int, int); 
extern unsigned appl_delconstraints (int, int *, int *);
extern unsigned appl_divide (int,
    long, long, double, double *, double, double *,
    int *, int *, int *, char *, double *,
    int *, int *, int *, int *, double *, char *, double *, char **,
    int, int, int);
extern unsigned appl_exit (int, double, double*);
extern unsigned appl_fathom (int, double, double);
extern unsigned appl_feasible (int, double, double *);
extern unsigned appl_init (int);
extern unsigned appl_initlp (int, int *);
extern unsigned appl_mps (int,
    int *, int *, int *, double **, double **, double **, char **,
    char **, double **, int **, int **, double **,
    int *, char **, char ***, int *, char **, char ***);
extern unsigned appl_node (int, long, long, double, double *);
extern unsigned appl_preprocessing (int);
extern unsigned appl_primal (int, double, double *, int, double, double *, double *, double *, int *);
extern unsigned appl_quit (int, double, double*);
extern unsigned appl_rank (int, long, long, double, double, double *);
extern unsigned appl_terminatenode (int, double, double, double *);
extern unsigned appl_terminatelp (int, double, double *, double *);
extern unsigned appl_variables (int,
    double, double *, double, double *,
    int *, int *, char *, double *, double *, double *,
    int *, int *, double *, char **, int, int);
#else
extern unsigned appl_bounds ();
extern unsigned appl_constraints ();
extern unsigned appl_delconstraints ();
extern unsigned appl_divide ();
extern unsigned appl_exit ();
extern unsigned appl_fathom ();
extern unsigned appl_feasible();
extern unsigned appl_init ();
extern unsigned appl_initlp ();
extern unsigned appl_mps ();
extern unsigned appl_node ();
extern unsigned appl_preprocessing ();
extern unsigned appl_primal ();
extern unsigned appl_quit ();
extern unsigned appl_rank ();
extern unsigned appl_terminatenode ();
extern unsigned appl_terminatelp ();
extern unsigned appl_variables ();
#endif

/*
 * FUNCTION PROTOTYPES OF OTHER FUNCTIONS
 */

#if defined(PROTOTYPING)
extern char *inq_file (void);
extern char *inq_prob (void);
extern void inq_form (void);
extern void inq_obj (void);
extern void inq_constr (int);
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
#else
extern char *inq_file ();
extern char *inq_prob ();
extern void inq_form ();
extern void inq_obj ();
extern void inq_constr ();
extern void inq_var ();
extern void set_obj ();
extern void set_constr ();
extern void set_var ();
extern int minto_cix ();
extern int minto_vix ();
extern int lp_vcnt ();
extern int lp_ccnt ();
extern double lp_slack ();
extern double lp_pi ();
extern double lp_rc ();
extern void lp_base ();
extern int lp_cix ();
extern int lp_vix ();
extern void ctrl_clique ();
extern void ctrl_implication ();
extern void ctrl_knapcov ();
extern void ctrl_gubcov ();
extern void ctrl_flowcov ();
extern void ctrl_output ();
extern void ctrl_lpmethod ();
#if defined(CPLEX)
extern void ctrl_lppricing ();
extern void ctrl_lppricelist ();
extern void ctrl_lpperturbconst ();
extern void ctrl_lppertrubmethod ();
extern void ctrl_lprefactorfreq ();
extern void ctrl_lpscalingmethod ();
#endif
extern void bypass_lp ();
extern void bypass_fathom ();
extern void bypass_integral ();
extern void wrt_prob ();
extern double stat_avnds ();
extern long stat_maxnds ();
extern long stat_evnds ();
extern long stat_depth ();
extern int stat_lpcnt ();
extern double stat_gap ();
extern int stat_maxlprows ();
extern int stat_maxlpcols ();
extern int stat_cliquecnt ();
extern int stat_implicationcnt ();
extern int stat_knapcovcnt ();
extern int stat_gubcovcnt ();
extern int stat_sknapcovcnt ();
extern int stat_flowcovcnt ();
extern double stat_time ();
#endif

