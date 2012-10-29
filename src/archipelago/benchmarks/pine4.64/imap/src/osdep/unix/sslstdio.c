/*
 * Program:	SSL standard I/O routines for server use
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	22 September 1998
 * Last Edited:	29 March 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Get character
 * Returns: character or EOF
 */

int PBIN (void)
{
  if (!sslstdio) return getchar ();
  if (!ssl_getdata (sslstdio->sslstream)) return EOF;
				/* one last byte available */
  sslstdio->sslstream->ictr--;
  return (int) *(sslstdio->sslstream->iptr)++;
}


/* Get string
 * Accepts: destination string pointer
 *	    number of bytes available
 * Returns: destination string pointer or NIL if EOF
 */

char *PSIN (char *s,int n)
{
  int i,c;
  if (start_tls) {		/* doing a start TLS? */
    ssl_server_init (start_tls);/* enter the mode */
    start_tls = NIL;		/* don't do this again */
  }
  if (!sslstdio) return fgets (s,n,stdin);
  for (i = c = 0, n-- ; (c != '\n') && (i < n); sslstdio->sslstream->ictr--) {
    if ((sslstdio->sslstream->ictr <= 0) && !ssl_getdata (sslstdio->sslstream))
      return NIL;		/* read error */
    c = s[i++] = *(sslstdio->sslstream->iptr)++;
  }
  s[i] = '\0';			/* tie off string */
  return s;
}


/* Get record
 * Accepts: destination string pointer
 *	    number of bytes to read
 * Returns: T if success, NIL otherwise
 */

long PSINR (char *s,unsigned long n)
{
  unsigned long i;
  if (start_tls) {		/* doing a start TLS? */
    ssl_server_init (start_tls);/* enter the mode */
    start_tls = NIL;		/* don't do this again */
  }
  if (sslstdio) return ssl_getbuffer (sslstdio->sslstream,n,s);
				/* non-SSL case */
  while (n && ((i = fread (s,1,n,stdin)) || (errno == EINTR))) s += i,n -= i;
  return n ? NIL : LONGT;
}


/* Wait for stdin input
 * Accepts: timeout in seconds
 * Returns: T if have input on stdin, else NIL
 */

long INWAIT (long seconds)
{
  return (sslstdio ? ssl_server_input_wait : server_input_wait) (seconds);
}

/* Put character
 * Accepts: character
 * Returns: character written or EOF
 */

int PBOUT (int c)
{
  if (!sslstdio) return putchar (c);
				/* flush buffer if full */
  if (!sslstdio->octr && PFLUSH ()) return EOF;
  sslstdio->octr--;		/* count down one character */
  *sslstdio->optr++ = c;	/* write character */
  return c;			/* return that character */
}


/* Put string
 * Accepts: destination string pointer
 * Returns: 0 or EOF if error
 */

int PSOUT (char *s)
{
  if (!sslstdio) return fputs (s,stdout);
  while (*s) {			/* flush buffer if full */
    if (!sslstdio->octr && PFLUSH ()) return EOF;
    *sslstdio->optr++ = *s++;	/* write one more character */
    sslstdio->octr--;		/* count down one character */
  }
  return 0;			/* success */
}

/* Put record
 * Accepts: source sized text
 * Returns: 0 or EOF if error
 */

int PSOUTR (SIZEDTEXT *s)
{
  unsigned char *t = s->data;
  unsigned long i = s->size;
  unsigned long j;
  if (sslstdio) while (i) {	/* until request satisfied */
				/* flush buffer if full */
    if (!sslstdio->octr && PFLUSH ()) break;
				/* blat as big a chucnk as we can */
    memcpy (sslstdio->optr,t,j = min (i,sslstdio->octr));
    sslstdio->optr += j;	/* account for chunk */
    sslstdio->octr -= j;
    t += j;
    i -= j;
  }
  else while (i && ((j = fwrite (t,1,i,stdout)) || (errno == EINTR)))
    t += j,i -= j;
  return i ? EOF : NIL;
}


/* Flush output
 * Returns: 0 or EOF if error
 */

int PFLUSH (void)
{
  if (!sslstdio) return fflush (stdout);
				/* force out buffer */
  if (!ssl_sout (sslstdio->sslstream,sslstdio->obuf,
		 SSLBUFLEN - sslstdio->octr)) return EOF;
				/* renew output buffer */
  sslstdio->optr = sslstdio->obuf;
  sslstdio->octr = SSLBUFLEN;
  return 0;			/* success */
}
