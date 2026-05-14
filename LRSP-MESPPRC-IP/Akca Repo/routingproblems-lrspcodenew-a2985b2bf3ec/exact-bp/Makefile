#
# TUNE FOR THE REQUESTED LP Solver
#  (This is configured for CPLEX on pbr.ie.lehigh.edu)
#

#MINTOLIBDIR = /usr/local/minto31-linux-cplex81/lib


#CPLEXLIBDIR = /usr/local/cplex/lib/x86_sles10_4.1/static_pic/
CPLEXLIBDIR = ${HOME}/cplex91/lib/x86_RHEL3.0_3.2/static_pic/
MINTOLIBDIR = ${HOME}/Minto-10-07/
CPLEX_DIR = ${HOME}/cplex91



#
DEFINES  = -DCPLEX -DVERSION81 -DUNIX -DLINUX
INCLUDES = -I$(CPLEX_DIR)/include 

CC       = gcc -m32
CXX      = g++ -m32
CFLAGS   = -Wall -g -m32 #-O2
LD       = g++ -m32
LDFLAGS  = -g

#
# BASIC MACRO DEFINITIONS
#

APPLNAME =   minto

APPLINCLS =  minto.h

APPLSRCS =   a_bounds.cpp \
             a_cons.cpp \
             a_delcon.cpp \
             a_divide.cpp \
             a_exit.cpp \
             a_fathom.cpp \
             a_feas.cpp \
             a_init.cpp \
             a_initlp.cpp \
             a_mps.cpp \
             a_node.cpp \
             a_prep.cpp \
             a_primal.cpp \
             a_quit.cpp \
             a_rank.cpp \
             a_termlp.cpp \
             a_termnd.cpp \
             a_vars.cpp \
	     minto.cpp \
             initcplex.cpp 

APPLSRCS2 =   GetPairNum.c \
             SearchForNodes.c \
             savingsort.c \
             heuristicColGen.c \
             e_shortestpath.c \
             pricingprob.c \
             InitColGenerator.c \
             GetIndexOfCust.c \
             label_sorting.c 

APPLOBJS =   $(APPLSRCS:.cpp=.o)
APPLOBJS2 =  $(APPLSRCS2:.c=.o)
#
# MAKE SURE THAT THE FIRST TARGET IS THE RIGHT ONE
#

first : $(APPLNAME)
#
# META RULE FOR MAKING .o's FROM .cpp's, I.E., COMPILATION
#

.cpp.o:
	$(CXX) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@;


#
# META RULE FOR MAKING .o's FROM .c's, I.E., COMPILATION
#

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@;


#
# RULES FOR LOADING/LINKING
#

# This is for minto/cplex

$(APPLNAME): $(APPLOBJS) $(APPLOBJS2)
	$(LD) $(LDFLAGS) -o $@ $(APPLOBJS) $(APPLOBJS2) -L$(MINTOLIBDIR) -lminto  -lmintocplex -L$(CPLEXLIBDIR) -lcplex -lm -lpthread

clean:
	rm -f $(APPLOBJS) $(APPLOBJS2)

#
# DEPENDENCIES
#

$(APPLOBJS) :  $(APPLINCLS)

$(APPLOBJS2) :  $(APPLINCLS)
