/*
 * Program:	SSL authentication/encryption module
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
 * Last Edited:	27 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#define crypt ssl_private_crypt
#include <x509.h>
#include <ssl.h>
#include <err.h>
#include <pem.h>
#include <buffer.h>
#include <bio.h>
#include <crypto.h>
#include <rand.h>
#undef crypt

#define SSLBUFLEN 8192
#define SSLCIPHERLIST "ALL:!LOW"


/* SSL I/O stream */

typedef struct ssl_stream {
  TCPSTREAM *tcpstream;		/* TCP stream */
  SSL_CTX *context;		/* SSL context */
  SSL *con;			/* SSL connection */
  int ictr;			/* input counter */
  char *iptr;			/* input pointer */
  char ibuf[SSLBUFLEN];		/* input buffer */
} SSLSTREAM;

#include "sslio.h"

/* Function prototypes */

static SSLSTREAM *ssl_start(TCPSTREAM *tstream,char *host,unsigned long flags);
static char *ssl_start_work (SSLSTREAM *stream,char *host,unsigned long flags);
static int ssl_open_verify (int ok,X509_STORE_CTX *ctx);
static char *ssl_extract_cn (char *name);
static long ssl_compare_hostnames (unsigned char *s,unsigned char *pat);
static long ssl_abort (SSLSTREAM *stream);
static RSA *ssl_genkey (SSL *con,int export,int keylength);


/* Secure Sockets Layer network driver dispatch */

static struct ssl_driver ssldriver = {
  ssl_open,			/* open connection */
  ssl_aopen,			/* open preauthenticated connection */
  ssl_getline,			/* get a line */
  ssl_getbuffer,		/* get a buffer */
  ssl_soutr,			/* output pushed data */
  ssl_sout,			/* output string */
  ssl_close,			/* close connection */
  ssl_host,			/* return host name */
  ssl_remotehost,		/* return remote host name */
  ssl_port,			/* return port number */
  ssl_localhost			/* return local host name */
};
				/* non-NIL if doing SSL primary I/O */
static SSLSTDIOSTREAM *sslstdio = NIL;
static char *start_tls = NIL;	/* non-NIL if start TLS requested */

/* One-time SSL initialization */

static int sslonceonly = 0;

void ssl_onceonlyinit (void)
{
  if (!sslonceonly++) {		/* only need to call it once */
    int fd;
    unsigned long i;
    char tmp[MAILTMPLEN];
    struct stat sbuf;
				/* if system doesn't have /dev/urandom */
    if (stat ("/dev/urandom",&sbuf)) {
      if ((fd = open (tmpnam (tmp),O_WRONLY|O_CREAT,0600)) < 0)
	i = (unsigned long) tmp;
      else {
	unlink (tmp);		/* don't need the file */
	fstat (fd,&sbuf);	/* get information about the file */
	i = sbuf.st_ino;	/* remember its inode */
	close (fd);		/* or its descriptor */
      }
				/* not great but it'll have to do */
      sprintf (tmp + strlen (tmp),"%.80s%lx%lx%lx",
	       tcp_serverhost (),i,
	       (unsigned long) (time (0) ^ gethostid ()),
	       (unsigned long) getpid ());
      RAND_seed (tmp,strlen (tmp));
    }
				/* apply runtime linkage */
    mail_parameters (NIL,SET_SSLDRIVER,(void *) &ssldriver);
    mail_parameters (NIL,SET_SSLSTART,(void *) ssl_start);
    SSL_library_init ();	/* add all algorithms */
  }
}

/* SSL open
 * Accepts: host name
 *	    contact service name
 *	    contact port number
 * Returns: SSL stream if success else NIL
 */

SSLSTREAM *ssl_open (char *host,char *service,unsigned long port)
{
  TCPSTREAM *stream = tcp_open (host,service,port);
  return stream ? ssl_start (stream,host,port) : NIL;
}


/* SSL authenticated open
 * Accepts: host name
 *	    service name
 *	    returned user name buffer
 * Returns: SSL stream if success else NIL
 */

SSLSTREAM *ssl_aopen (NETMBX *mb,char *service,char *usrbuf)
{
  return NIL;			/* don't use this mechanism with SSL */
}

/* Start SSL/TLS negotiations
 * Accepts: open TCP stream of session
 *	    user's host name
 *	    flags
 * Returns: SSL stream if success else NIL
 */

static SSLSTREAM *ssl_start (TCPSTREAM *tstream,char *host,unsigned long flags)
{
  char *reason,tmp[MAILTMPLEN];
  sslfailure_t sf = (sslfailure_t) mail_parameters (NIL,GET_SSLFAILURE,NIL);
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  void *data = (*bn) (BLOCK_SENSITIVE,NIL);
  SSLSTREAM *stream = (SSLSTREAM *) memset (fs_get (sizeof (SSLSTREAM)),0,
					    sizeof (SSLSTREAM));
  stream->tcpstream = tstream;	/* bind TCP stream */
				/* do the work */
  reason = ssl_start_work (stream,host,flags);
  (*bn) (BLOCK_NONSENSITIVE,data);
  if (reason) {			/* failed? */
    ssl_close (stream);		/* failed to do SSL */
    stream = NIL;		/* no stream returned */
    switch (*reason) {		/* analyze reason */
    case '*':			/* certificate failure */
      ++reason;			/* skip over certificate failure indication */
				/* pass to error callback */
      if (sf) (*sf) (host,reason,flags);
      else {			/* no error callback, build error message */
	sprintf (tmp,"Certificate failure for %.80s: %.512s",host,reason);
	mm_log (tmp,ERROR);
      }
    case '\0':			/* user answered no to certificate callback */
      if (flags & NET_TRYSSL)	/* return dummy stream to stop tryssl */
	stream = (SSLSTREAM *) memset (fs_get (sizeof (SSLSTREAM)),0,
				       sizeof (SSLSTREAM));
      break;
    default:			/* non-certificate failure */
      if (flags & NET_TRYSSL);	/* no error output if tryssl */
				/* pass to error callback */
      else if (sf) (*sf) (host,reason,flags);
      else {			/* no error callback, build error message */
	sprintf (tmp,"TLS/SSL failure for %.80s: %.512s",host,reason);
	mm_log (tmp,ERROR);
      }
      break;
    }
  }
  return stream;
}

/* Start SSL/TLS negotiations worker routine
 * Accepts: SSL stream
 *	    user's host name
 *	    flags
 * Returns: NIL if success, else error reason
 */

				/* evil but I had no choice */
static char *ssl_last_error = NIL;
static char *ssl_last_host = NIL;

static char *ssl_start_work (SSLSTREAM *stream,char *host,unsigned long flags)
{
  BIO *bio;
  X509 *cert;
  char *s,*err,tmp[MAILTMPLEN];
  sslcertificatequery_t scq =
    (sslcertificatequery_t) mail_parameters (NIL,GET_SSLCERTIFICATEQUERY,NIL);
  if (ssl_last_error) fs_give ((void **) &ssl_last_error);
  ssl_last_host = host;
  if (!(stream->context = SSL_CTX_new ((flags & NET_TLSCLIENT) ?
				       TLSv1_client_method () :
				       SSLv23_client_method ())))
    return "SSL context failed";
  SSL_CTX_set_options (stream->context,0);
				/* disable certificate validation? */
  if (flags & NET_NOVALIDATECERT)
    SSL_CTX_set_verify (stream->context,SSL_VERIFY_NONE,NIL);
  else SSL_CTX_set_verify (stream->context,SSL_VERIFY_PEER,ssl_open_verify);
				/* set default paths to CAs */
  SSL_CTX_set_default_verify_paths (stream->context);
				/* create connection */
  if (!(stream->con = (SSL *) SSL_new (stream->context)))
    return "SSL connection failed";
  bio = BIO_new_socket (stream->tcpstream->tcpsi,BIO_NOCLOSE);
  SSL_set_bio (stream->con,bio,bio);
  SSL_set_connect_state (stream->con);
  if (SSL_in_init (stream->con)) SSL_total_renegotiations (stream->con);
				/* now negotiate SSL */
  if (SSL_write (stream->con,"",0) < 0)
    return ssl_last_error ? ssl_last_error : "SSL negotiation failed";
				/* need to validate host names? */
  if (!(flags & NET_NOVALIDATECERT)) {
				/* get certificate */
    if (!(cert = SSL_get_peer_certificate (stream->con)))
      err = "No certificate from server";
				/* locate Common Name */
    else if (!(s = ssl_extract_cn (cert->name)))
      err = "Unable to locate common name in certificate";
				/* wildcard match it with user's name */
    else err = ssl_compare_hostnames (host,s) ? NIL :
      "Server name does not match certificate";
    if (err) {			/* got an error? */
				/* application callback */
      if (scq) return (*scq) (err,host,cert->name) ? NIL : "";
				/* error message to return via mm_log() */
      sprintf (tmp,"*%.128s: %.255s",err,cert->name);
      return ssl_last_error = cpystr (tmp);
    }
  }
  return NIL;
}

/* SSL certificate verification callback
 * Accepts: error flag
 *	    X509 context
 * Returns: error flag
 */

static int ssl_open_verify (int ok,X509_STORE_CTX *ctx)
{
  char *err,cert[256],tmp[MAILTMPLEN];
  sslcertificatequery_t scq =
    (sslcertificatequery_t) mail_parameters (NIL,GET_SSLCERTIFICATEQUERY,NIL);
  if (!ok) {			/* in case failure */
    err = (char *) X509_verify_cert_error_string
      (X509_STORE_CTX_get_error (ctx));
    X509_NAME_oneline (X509_get_subject_name
		       (X509_STORE_CTX_get_current_cert (ctx)),cert,255);
    if (!scq) {			/* mm_log() error message if no callback */
      sprintf (tmp,"*%.128s: %.255s",err,cert);
      ssl_last_error = cpystr (tmp);
    }
				/* ignore error if application says to */
    else if ((*scq) (err,ssl_last_host,cert)) ok = T;
				/* application wants punt */
    else ssl_last_error = cpystr ("");
  }
  return ok;
}


/* SSL extract Common Name
 * Accepts: name
 * Returns: common name, or NIL if failure
 */

static char *ssl_extract_cn (char *name)
{
  char *s;
  if (name && (name = strstr (name,"/CN=")) && (s = strchr (name += 4,'/')))
    *s = '\0';
  return name;
}


/* Case-independent wildcard pattern match
 * Accepts: base string
 *	    pattern string
 * Returns: T if pattern matches base, else NIL
 */

static long ssl_compare_hostnames (unsigned char *s,unsigned char *pat)
{
  if (*pat != '*')		/* non-wildcard */
    return ((isupper (*pat) ? tolower (*pat) : *pat) ==
	    (isupper (*s) ? tolower (*s) : *s)) ?
	      (*pat ? ssl_compare_hostnames (s+1,pat+1) : T) : NIL;
  if (pat[1]) {			/* scan remainder of string until delimiter */
    do if (ssl_compare_hostnames (s,pat+1)) return T;
    while ((*s != '.') && *s++);
  }
  return NIL;			/* wildcard ran off end of string */
}

/* SSL receive line
 * Accepts: SSL stream
 * Returns: text line string or NIL if failure
 */

char *ssl_getline (SSLSTREAM *stream)
{
  int n,m;
  char *st,*ret,*stp;
  char c = '\0';
  char d;
				/* make sure have data */
  if (!ssl_getdata (stream)) return NIL;
  st = stream->iptr;		/* save start of string */
  n = 0;			/* init string count */
  while (stream->ictr--) {	/* look for end of line */
    d = *stream->iptr++;	/* slurp another character */
    if ((c == '\015') && (d == '\012')) {
      ret = (char *) fs_get (n--);
      memcpy (ret,st,n);	/* copy into a free storage string */
      ret[n] = '\0';		/* tie off string with null */
      return ret;
    }
    n++;			/* count another character searched */
    c = d;			/* remember previous character */
  }
				/* copy partial string from buffer */
  memcpy ((ret = stp = (char *) fs_get (n)),st,n);
				/* get more data from the net */
  if (!ssl_getdata (stream)) fs_give ((void **) &ret);
				/* special case of newline broken by buffer */
  else if ((c == '\015') && (*stream->iptr == '\012')) {
    stream->iptr++;		/* eat the line feed */
    stream->ictr--;
    ret[n - 1] = '\0';		/* tie off string with null */
  }
				/* else recurse to get remainder */
  else if (st = ssl_getline (stream)) {
    ret = (char *) fs_get (n + 1 + (m = strlen (st)));
    memcpy (ret,stp,n);		/* copy first part */
    memcpy (ret + n,st,m);	/* and second part */
    fs_give ((void **) &stp);	/* flush first part */
    fs_give ((void **) &st);	/* flush second part */
    ret[n + m] = '\0';		/* tie off string with null */
  }
  return ret;
}

/* SSL receive buffer
 * Accepts: SSL stream
 *	    size in bytes
 *	    buffer to read into
 * Returns: T if success, NIL otherwise
 */

long ssl_getbuffer (SSLSTREAM *stream,unsigned long size,char *buffer)
{
  unsigned long n;
  while (size > 0) {		/* until request satisfied */
    if (!ssl_getdata (stream)) return NIL;
    n = min (size,stream->ictr);/* number of bytes to transfer */
				/* do the copy */
    memcpy (buffer,stream->iptr,n);
    buffer += n;		/* update pointer */
    stream->iptr += n;
    size -= n;			/* update # of bytes to do */
    stream->ictr -= n;
  }
  buffer[0] = '\0';		/* tie off string */
  return T;
}

/* SSL receive data
 * Accepts: TCP/IP stream
 * Returns: T if success, NIL otherwise
 */

long ssl_getdata (SSLSTREAM *stream)
{
  int i,sock;
  fd_set fds,efds;
  struct timeval tmo;
  tcptimeout_t tmoh = (tcptimeout_t) mail_parameters (NIL,GET_TIMEOUT,NIL);
  long ttmo_read = (long) mail_parameters (NIL,GET_READTIMEOUT,NIL);
  time_t t = time (0);
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  if (!stream->con || ((sock = SSL_get_fd (stream->con)) < 0)) return NIL;
  (*bn) (BLOCK_TCPREAD,NIL);
  while (stream->ictr < 1) {	/* if nothing in the buffer */
    if (!SSL_pending (stream->con)) {
      time_t tl = time (0);	/* start of request */
      time_t now = tl;
      int ti = ttmo_read ? now + ttmo_read : 0;
      tmo.tv_usec = 0;
      FD_ZERO (&fds);		/* initialize selection vector */
      FD_ZERO (&efds);		/* handle errors too */
      FD_SET (sock,&fds);	/* set bit in selection vector */
      FD_SET (sock,&efds);	/* set bit in error selection vector */
      errno = NIL;		/* block and read */
      do {			/* block under timeout */
	tmo.tv_sec = ti ? ti - now : 0;
	i = select (sock+1,&fds,0,&efds,ti ? &tmo : 0);
	now = time (0);		/* fake timeout if interrupt & time expired */
	if ((i < 0) && (errno == EINTR) && ti && (ti <= now)) i = 0;
      } while ((i < 0) && (errno == EINTR));
      if (i <= 0) {		/* failed, timeout with continue? */
	if (!i && tmoh && ((*tmoh) (now - t,now - tl))) continue;
				/* error or timeout no-continue */
	else return ssl_abort (stream);
      }
    }
    while (((i = SSL_read (stream->con,stream->ibuf,SSLBUFLEN)) < 0) &&
	   ((errno == EINTR) ||
	    (SSL_get_error (stream->con,i) == SSL_ERROR_WANT_READ)));
    if (i < 1) return ssl_abort (stream);
    stream->iptr = stream->ibuf;/* point at TCP buffer */
    stream->ictr = i;		/* set new byte count */
  }
  (*bn) (BLOCK_NONE,NIL);
  return T;
}

/* SSL send string as record
 * Accepts: SSL stream
 *	    string pointer
 * Returns: T if success else NIL
 */

long ssl_soutr (SSLSTREAM *stream,char *string)
{
  return ssl_sout (stream,string,(unsigned long) strlen (string));
}


/* SSL send string
 * Accepts: SSL stream
 *	    string pointer
 *	    byte count
 * Returns: T if success else NIL
 */

long ssl_sout (SSLSTREAM *stream,char *string,unsigned long size)
{
  long i;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  if (!stream->con) return NIL;
  (*bn) (BLOCK_TCPWRITE,NIL);
				/* until request satisfied */
  for (i = 0; size > 0; string += i,size -= i)
				/* write as much as we can */
    if ((i = SSL_write (stream->con,string,(int) min (SSLBUFLEN,size))) < 0)
      return ssl_abort (stream);/* write failed */
  (*bn) (BLOCK_NONE,NIL);
  return LONGT;			/* all done */
}

/* SSL close
 * Accepts: SSL stream
 */

void ssl_close (SSLSTREAM *stream)
{
  ssl_abort (stream);		/* nuke the stream */
  fs_give ((void **) &stream);	/* flush the stream */
}


/* SSL abort stream
 * Accepts: SSL stream
 * Returns: NIL always
 */

static long ssl_abort (SSLSTREAM *stream)
{
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  if (stream->con) {		/* close SSL connection */
    SSL_shutdown (stream->con);
    SSL_free (stream->con);
    stream->con = NIL;
  }
  if (stream->context) {	/* clean up context */
    SSL_CTX_free (stream->context);
    stream->context = NIL;
  }
  if (stream->tcpstream) {	/* close TCP stream */
    tcp_close (stream->tcpstream);
    stream->tcpstream = NIL;
  }
  (*bn) (BLOCK_NONE,NIL);
  return NIL;
}

/* SSL get host name
 * Accepts: SSL stream
 * Returns: host name for this stream
 */

char *ssl_host (SSLSTREAM *stream)
{
  return tcp_host (stream->tcpstream);
}


/* SSL get remote host name
 * Accepts: SSL stream
 * Returns: host name for this stream
 */

char *ssl_remotehost (SSLSTREAM *stream)
{
  return tcp_remotehost (stream->tcpstream);
}


/* SSL return port for this stream
 * Accepts: SSL stream
 * Returns: port number for this stream
 */

unsigned long ssl_port (SSLSTREAM *stream)
{
  return tcp_port (stream->tcpstream);
}


/* SSL get local host name
 * Accepts: SSL stream
 * Returns: local host name
 */

char *ssl_localhost (SSLSTREAM *stream)
{
  return tcp_localhost (stream->tcpstream);
}

/* Start TLS
 * Accepts: /etc/services service name
 * Returns: cpystr'd error string if TLS failed, else NIL for success
 */

char *ssl_start_tls (char *server)
{
  char tmp[MAILTMPLEN];
  struct stat sbuf;
  if (sslstdio) return cpystr ("Already in an SSL session");
  if (start_tls) return cpystr ("TLS already started");
  if (server) {			/* build specific certificate/key file name */
    sprintf (tmp,"%s/%s-%s.pem",SSL_CERT_DIRECTORY,server,tcp_serveraddr ());
    if (stat (tmp,&sbuf)) {	/* use non-specific name if no specific file */
      sprintf (tmp,"%s/%s.pem",SSL_CERT_DIRECTORY,server);
      if (stat (tmp,&sbuf)) return cpystr ("Server certificate not installed");
    }
    start_tls = server;		/* switch to STARTTLS mode */
  }
  return NIL;
}

/* Init server for SSL
 * Accepts: server name
 */

void ssl_server_init (char *server)
{
  char cert[MAILTMPLEN],key[MAILTMPLEN];
  unsigned long i;
  struct stat sbuf;
  SSLSTREAM *stream = (SSLSTREAM *) memset (fs_get (sizeof (SSLSTREAM)),0,
					    sizeof (SSLSTREAM));
  ssl_onceonlyinit ();		/* make sure algorithms added */
  ERR_load_crypto_strings ();
  SSL_load_error_strings ();
				/* build specific certificate/key file names */
  sprintf (cert,"%s/%s-%s.pem",SSL_CERT_DIRECTORY,server,tcp_serveraddr ());
  sprintf (key,"%s/%s-%s.pem",SSL_KEY_DIRECTORY,server,tcp_serveraddr ());
				/* use non-specific name if no specific cert */
  if (stat (cert,&sbuf)) sprintf (cert,"%s/%s.pem",SSL_CERT_DIRECTORY,server);
  if (stat (key,&sbuf)) {	/* use non-specific name if no specific key */
    sprintf (key,"%s/%s.pem",SSL_KEY_DIRECTORY,server);
				/* use cert file as fallback for key */
    if (stat (key,&sbuf)) strcpy (key,cert);
  }
				/* create context */
  if (!(stream->context = SSL_CTX_new (start_tls ?
				       TLSv1_server_method () :
				       SSLv23_server_method ())))
    syslog (LOG_ALERT,"Unable to create SSL context, host=%.80s",
	    tcp_clienthost ());
  else {			/* set context options */
    SSL_CTX_set_options (stream->context,SSL_OP_ALL);
				/* set cipher list */
    if (!SSL_CTX_set_cipher_list (stream->context,SSLCIPHERLIST))
      syslog (LOG_ALERT,"Unable to set cipher list %.80s, host=%.80s",
	      SSLCIPHERLIST,tcp_clienthost ());
				/* load certificate */
    else if (!SSL_CTX_use_certificate_chain_file (stream->context,cert))
      syslog (LOG_ALERT,"Unable to load certificate from %.80s, host=%.80s",
	      cert,tcp_clienthost ());
				/* load key */
    else if (!(SSL_CTX_use_RSAPrivateKey_file (stream->context,key,
					       SSL_FILETYPE_PEM)))
      syslog (LOG_ALERT,"Unable to load private key from %.80s, host=%.80s",
	      key,tcp_clienthost ());

    else {			/* generate key if needed */
      if (SSL_CTX_need_tmp_RSA (stream->context))
	SSL_CTX_set_tmp_rsa_callback (stream->context,ssl_genkey);
				/* create new SSL connection */
      if (!(stream->con = SSL_new (stream->context)))
	syslog (LOG_ALERT,"Unable to create SSL connection, host=%.80s",
		tcp_clienthost ());
      else {			/* set file descriptor */
	SSL_set_fd (stream->con,0);
				/* all OK if accepted */
	if (SSL_accept (stream->con) < 0)
	  syslog (LOG_INFO,"Unable to accept SSL connection, host=%.80s",
		  tcp_clienthost ());
	else {			/* server set up */
	  sslstdio = (SSLSTDIOSTREAM *)
	    memset (fs_get (sizeof(SSLSTDIOSTREAM)),0,sizeof (SSLSTDIOSTREAM));
	  sslstdio->sslstream = stream;
				/* available space in output buffer */
	  sslstdio->octr = SSLBUFLEN;
				/* current output buffer pointer */
	  sslstdio->optr = sslstdio->obuf;
				/* allow PLAIN authenticator */
	  auth_pla.server = auth_plain_server;
	  if ((long) mail_parameters (NIL,GET_DISABLEPLAINTEXT,NIL) > 1)
	    mail_parameters (NIL,SET_DISABLEPLAINTEXT,NIL);
	  return;
	}
      }
    }  
  }
  while (i = ERR_get_error ())	/* SSL failure */
    syslog (LOG_ERR,"SSL error status: %.80s",ERR_error_string (i,NIL));
  ssl_close (stream);		/* punt stream */
  exit (1);			/* punt this program too */
}

/* Generate one-time key for server
 * Accepts: SSL connection
 *	    export flag
 *	    keylength
 * Returns: generated key, always
 */

static RSA *ssl_genkey (SSL *con,int export,int keylength)
{
  unsigned long i;
  static RSA *key = NIL;
  if (!key) {			/* if don't have a key already */
				/* generate key */
    if (!(key = RSA_generate_key (export ? keylength : 1024,RSA_F4,NIL,NIL))) {
      syslog (LOG_ALERT,"Unable to generate temp key, host=%.80s",
	      tcp_clienthost ());
      while (i = ERR_get_error ())
	syslog (LOG_ALERT,"SSL error status: %s",ERR_error_string (i,NIL));
      exit (1);
    }
  }
  return key;
}

/* Wait for stdin input
 * Accepts: timeout in seconds
 * Returns: T if have input on stdin, else NIL
 */

long ssl_server_input_wait (long seconds)
{
  int i,sock;
  fd_set fds,efd;
  struct timeval tmo;
  SSLSTREAM *stream;
  if (!sslstdio) return server_input_wait (seconds);
				/* input available in buffer */
  if (((stream = sslstdio->sslstream)->ictr > 0) ||
      !stream->con || ((sock = SSL_get_fd (stream->con)) < 0)) return LONGT;
				/* input available from SSL */
  if (SSL_pending (stream->con) &&
      ((i = SSL_read (stream->con,stream->ibuf,SSLBUFLEN)) > 0)) {
    stream->iptr = stream->ibuf;/* point at TCP buffer */
    stream->ictr = i;		/* set new byte count */
    return LONGT;
  }
  FD_ZERO (&fds);		/* initialize selection vector */
  FD_ZERO (&efd);		/* initialize selection vector */
  FD_SET (sock,&fds);		/* set bit in selection vector */
  FD_SET (sock,&efd);		/* set bit in selection vector */
  tmo.tv_sec = seconds; tmo.tv_usec = 0;
				/* see if input available from the socket */
  return select (sock+1,&fds,0,&efd,&tmo) ? LONGT : NIL;
}

#include "sslstdio.c"
