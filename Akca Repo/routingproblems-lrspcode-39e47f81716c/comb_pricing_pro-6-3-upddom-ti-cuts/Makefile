#
# TUNE FOR THE REQUESTED LP Solver
#  (This is configured for CPLEX on pbr.ie.lehigh.edu)
#

#MINTOLIBDIR = /usr/local/minto31-linux-cplex81/lib

CPLEXLIBDIR =/usr/local/cplex91/lib/x86_RHEL3.0_3.2/static_pic/
#MINTOLIBDIR =${HOME}/BranchPrice/mintofiles
MINTOLIBDIR =${HOME}/mintolib-new

#CPLEXLIBDIR = /usr/local/ilog/cplex91/lib/x86_RHEL3.0_3.2/static_pic
#MINTOLIBDIR =${HOME}/Fire_Backup/lib_minto

#
DEFINES  = -DCPLEX -DVERSION81 -DUNIX -DLINUX
INCLUDES = 

CC       = gcc
CFLAGS   = -Wall -g #-O2
LD       = gcc
LDFLAGS  = 

#
# BASIC MACRO DEFINITIONS
#

APPLNAME =   minto

APPLINCLS =  minto.h

APPLSRCS =   minto.c \
             a_bounds.c \
             a_cons.c \
             a_delcon.c \
             a_divide.c \
             a_exit.c \
             a_fathom.c \
             a_feas.c \
             a_init.c \
             a_initlp.c \
             a_mps.c \
             a_node.c \
             a_prep.c \
             a_primal.c \
             a_quit.c \
             a_rank.c \
             a_termlp.c \
             a_termnd.c \
             a_vars.c \
             GetPairNum.c \
             SearchForNodes.c \
             updatedheursoln.c \
             savingsort.c \
             heuristicColGen.c \
             e_shortestpath.c \
             pricingprob.c \
             GetIndexOfCust.c	

APPLOBJS =   minto.o \
             a_bounds.o \
             a_cons.o \
             a_delcon.o \
             a_divide.o \
             a_exit.o \
             a_fathom.o \
             a_feas.o \
             a_init.o \
             a_initlp.o \
             a_mps.o \
             a_node.o \
             a_prep.o \
             a_primal.o \
             a_quit.o \
             a_rank.o \
             a_termlp.o \
             a_termnd.o \
             a_vars.o \
             GetPairNum.o \
             SearchForNodes.o \
             updatedheursoln.o \
             savingsort.o \
             heuristicColGen.o \
             e_shortestpath.o \
             pricingprob.o \
             GetIndexOfCust.o
#
# MAKE SURE THAT THE FIRST TARGET IS THE RIGHT ONE
#

first : $(APPLNAME)

#
# META RULE FOR MAKING .o's FROM .c's, I.E., COMPILATION
#

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< ;

#
# RULES FOR LOADING/LINKING
#

# This is for minto/cplex

$(APPLNAME): $(APPLOBJS)
	$(LD) $(LDFLAGS) -o $@ $(APPLOBJS) -L$(MINTOLIBDIR) -lminto  -L$(CPLEXLIBDIR) -lcplex -lm -lpthread

clean:
	rm -f $(APPLOBJS)

#
# DEPENDENCIES
#

$(APPLOBJS) :  $(APPLINCLS)
