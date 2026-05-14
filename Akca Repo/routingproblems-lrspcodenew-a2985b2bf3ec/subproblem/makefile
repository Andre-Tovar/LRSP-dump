CC       = gcc 
CFLAGS   = -g 
LD       = gcc 
LDFLAGS  = -g

#
# BASIC MACRO DEFINITIONS
#

APPLNAME =   prog

APPLSRCS =   mespprc.c \
             espprc.c
             
APPLOBJS =   $(APPLSRCS:.c=.o)

#
# MAKE SURE THAT THE FIRST TARGET IS THE RIGHT ONE
#

first : $(APPLNAME)

#
# META RULE FOR MAKING .o's FROM .c's, I.E., COMPILATION
#

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@;


#
# RULES FOR LOADING/LINKING
#

# This is for minto/cplex

$(APPLNAME): $(APPLOBJS) 
	$(LD) $(LDFLAGS) -o $@ $(APPLOBJS) -lm 

clean:
	rm -f $(APPLOBJS) 

