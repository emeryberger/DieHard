#include <stdio.h>
#include <time.h>

/*
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-1996 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 */

main(argc, argv)
    int    argc;
    char **argv;
{
    struct tm *t;
    FILE      *outfile=stdout;
    time_t     ltime;
#ifdef OS2
    char       hostname[256];
 
    gethostname(hostname, sizeof hostname - 1);
#endif

    if(argc > 1 && (outfile = fopen(argv[1], "w")) == NULL){
	fprintf(stderr, "can't create '%s'\n", argv[1]);
	exit(1);
    }

    time(&ltime);
    t = localtime(&ltime);
    fprintf(outfile,"char datestamp[]=\"%02d-%s-%d\";\n", t->tm_mday, 
	    (t->tm_mon == 0) ? "Jan" : 
	     (t->tm_mon == 1) ? "Feb" : 
	      (t->tm_mon == 2) ? "Mar" : 
	       (t->tm_mon == 3) ? "Apr" : 
		(t->tm_mon == 4) ? "May" : 
		 (t->tm_mon == 5) ? "Jun" : 
		  (t->tm_mon == 6) ? "Jul" : 
		   (t->tm_mon == 7) ? "Aug" : 
		    (t->tm_mon == 8) ? "Sep" : 
		     (t->tm_mon == 9) ? "Oct" : 
		      (t->tm_mon == 10) ? "Nov" : 
		       (t->tm_mon == 11) ? "Dec" : "UKN",
	    1900 + t->tm_year);
#ifdef OS2
    fprintf(outfile, "char hoststamp[]=\"%s\";\n", hostname);
#else
    fprintf(outfile, "char hoststamp[]=\"random-pc\";\n");
#endif
    exit(0);
}
