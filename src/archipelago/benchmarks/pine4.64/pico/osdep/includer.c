#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: includer.c 11688 2001-06-21 17:54:43Z hubert $";
#endif
#include <stdio.h>
#include <string.h>

/*
 * Inflexible cat with include files.
 * The include lines must look exactly like  include(filename)  with no
 * spaces before the include, or between the parens and the surrounding
 * characters.
 *
 * This probably ought to just be a script that uses "cat".
 */
main(argc, argv)
int argc;
char *argv[];
{
  FILE * infile = stdin;
  FILE * outfile = stdout;

  if (argc > 1) {
    if ((infile = fopen(argv[1], "r")) == NULL) {
      fprintf(stderr, "includer: can't open '%s' for input\n", argv[1]);
      exit(1);
    }
    if (argc > 2) {
      if ((outfile = fopen(argv[2], "w")) == NULL) {
	fprintf(stderr, "includer: can't create '%s'\n", argv[2]);
	exit(1);
      }
    }
  }
  readfile(infile, outfile);
  exit(0);
}

readfile(fpin, fpout)
FILE *fpin, *fpout;
{
    char line[BUFSIZ+1];
    FILE *fp;
    char *p, *fname, *fend;

    while ((p = fgets(line, BUFSIZ, fpin)) != NULL) {

        if (!strncmp("include(", p, strlen("include("))) {

            /* do include */
            fname = strchr(p, '(');
            if (fname == NULL) {
                fprintf(stderr, "Can't find include file %s\n", p);
                exit(1);
            }
            fname++;
            fend = strrchr(fname, ')');
            if (fend == NULL) {
                fprintf(stderr, "Can't find include file %s\n", p);
                exit(1);
            }
            *fend = '\0';
            if ((fp = fopen(fname, "r")) == NULL) {
                fprintf(stderr, "Can't open include file %s\n", fname);
                exit(1);
            }
            readfile(fp, fpout);
            fclose(fp);

        /* skip if comment line (begins with ;) */
        }else if (*p != ';') {
            fputs(p, fpout);
        }
    }

    return(1);
}
