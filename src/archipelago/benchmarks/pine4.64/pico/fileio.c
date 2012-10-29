#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: fileio.c 12094 2002-02-12 22:53:53Z hubert $";
#endif
/*
 * Program:	ASCII file reading routines
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2002 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 */
/*
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here. A better message writing scheme should
 * be used.
 */
#include	"headers.h"


#if	defined(bsd) || defined(lnx)
extern int errno;
#endif


FIOINFO	g_pico_fio;

/*
 * Open a file for reading.
 */
ffropen(fn)
  char    *fn;
{
    int status;

    if ((status = fexist(g_pico_fio.name = fn, "r", (off_t *)NULL)) == FIOSUC){
	g_pico_fio.flags = FIOINFO_READ;
	if((g_pico_fio.fp = fopen(g_pico_fio.name, "r")) == NULL)
	  status = FIOFNF;
    }

    return (status);
}


/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Check only at the newline.
 */
ffputline(buf, nbuf)
    CELL  buf[];
    int	  nbuf;
{
    register int    i;

    for (i = 0; i < nbuf; ++i)
       if(fputc(buf[i].c&0xFF, g_pico_fio.fp) == EOF)
	 break;

   if(i == nbuf)
      fputc('\n', g_pico_fio.fp);

    if (ferror(g_pico_fio.fp)) {
        emlwrite("\007Write error: %s", errstr(errno));
	sleep(5);
        return (FIOERR);
    }

    return (FIOSUC);
}



/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 */
ffgetline(buf, nbuf, charsreturned, msg)
  register char   buf[];
  int nbuf;
  int *charsreturned;
  int msg;
{
    register int    c;
    register int    i;

    if(charsreturned)
      *charsreturned = 0;

    i = 0;

    while ((c = fgetc(g_pico_fio.fp)) != EOF && c != '\n') {
	/*
	 * Don't blat the CR should the newline be CRLF and we're
	 * running on a unix system.  NOTE: this takes care of itself
	 * under DOS since the non-binary open turns newlines into '\n'.
	 */
	if(c == '\r'){
	    if((c = fgetc(g_pico_fio.fp)) == EOF || c == '\n')
	      break;

	    if (i < nbuf-2)		/* Bare CR. Insert it and go on... */
	      buf[i++] = '\r';		/* else, we're up a creek */
	}

        if (i >= nbuf-2) {
	    buf[nbuf - 2] = c;	/* store last char read */
	    buf[nbuf - 1] = 0;	/* and terminate it */
	    if(charsreturned)
	      *charsreturned = nbuf - 1;
	    if (msg)
	      emlwrite("File has long line", NULL);
            return (FIOLNG);
        }
        buf[i++] = c;
    }

    if (c == EOF) {
        if (ferror(g_pico_fio.fp)) {
            emlwrite("File read error", NULL);
	    if(charsreturned)
	      *charsreturned = i;
            return (FIOERR);
        }

        if (i != 0)
	  emlwrite("File doesn't end with newline.  Adding one.", NULL);
	else{
	    if(charsreturned)
	      *charsreturned = i;
	    return (FIOEOF);
	}
    }

    buf[i] = 0;
    if(charsreturned)
      *charsreturned = i;
    return (FIOSUC);
}
