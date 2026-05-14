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


#include "header.h"

#ifdef PROTOTYPING
void main (int, char **);
extern void minto (char *, char *);
#else
void main ();
extern void minto ();
#endif

/*
 * main ()
 */

void
main (argc, argv)
int argc;
char **argv;
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

}
