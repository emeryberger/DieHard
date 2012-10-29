/*
 * Program:	Winsock TCP/IP routines
 *
 * Author:	Mark Crispin from Mike Seibel's Winsock code
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 April 1989
 * Last Edited: 8 August 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include "ip_nt.c"


#define TCPMAXSEND 32768

/* Private functions */

int tcp_socket_open (int family,void *adr,size_t adrlen,unsigned short port,
		     char *tmp,char *hst);
long tcp_abort (SOCKET *sock);
char *tcp_name (struct sockaddr *sadr,long flag);
char *tcp_name_valid (char *s);


/* Private data */

int wsa_initted = 0;		/* init ? */
static int wsa_sock_open = 0;	/* keep track of open sockets */
static tcptimeout_t tmoh = NIL;	/* TCP timeout handler routine */
static long ttmo_read = 0;	/* TCP timeouts, in seconds */
static long ttmo_write = 0;
static long allowreversedns = T;/* allow reverse DNS lookup */
static long tcpdebug = NIL;	/* extra TCP debugging telemetry */

/* TCP/IP manipulate parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *tcp_parameters (long function,void *value)
{
  void *ret = NIL;
  switch ((int) function) {
  case SET_TIMEOUT:
    tmoh = (tcptimeout_t) value;
  case GET_TIMEOUT:
    ret = (void *) tmoh;
    break;
  case SET_READTIMEOUT:
    ttmo_read = (long) value;
  case GET_READTIMEOUT:
    ret = (void *) ttmo_read;
    break;
  case SET_WRITETIMEOUT:
    ttmo_write = (long) value;
  case GET_WRITETIMEOUT:
    ret = (void *) ttmo_write;
    break;
  case SET_ALLOWREVERSEDNS:
    allowreversedns = (long) value;
  case GET_ALLOWREVERSEDNS:
    ret = (void *) allowreversedns;
    break;
  case SET_TCPDEBUG:
    tcpdebug = (long) value;
  case GET_TCPDEBUG:
    ret = (void *) tcpdebug;
    break;
  }
  return ret;
}

/* TCP/IP open
 * Accepts: host name
 *	    contact service name
 *	    contact port number and optional silent flag
 * Returns: TCP/IP stream if success else NIL
 */

TCPSTREAM *tcp_open (char *host,char *service,unsigned long port)
{
  TCPSTREAM *stream = NIL;
  int i,family;
  SOCKET sock = INVALID_SOCKET;
  int silent = (port & NET_SILENT) ? T : NIL;
  char *s,*hostname,tmp[MAILTMPLEN];
  void *adr,*next;
  size_t adrlen;
  struct servent *sv = NIL;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  if (!wsa_initted++) {		/* init Windows Sockets */
    WSADATA wsock;
    if (i = (int) WSAStartup (WINSOCK_VERSION,&wsock)) {
      wsa_initted = 0;		/* in case we try again */
      sprintf (tmp,"Unable to start Windows Sockets (%d)",i);
      mm_log (tmp,ERROR);
      return NIL;
    }
  }
  port &= 0xffff;		/* erase flags */
				/* lookup service */
  if (service && (sv = getservbyname (service,"tcp")))
    port = ntohs (sv->s_port);
  /* The domain literal form is used (rather than simply the dotted decimal
     as with other Windows programs) because it has to be a valid "host name"
     in mailsystem terminology. */
				/* look like domain literal? */
  if (host[0] == '[' && host[(strlen (host))-1] == ']') {
    strcpy (tmp,host+1);	/* yes, copy number part */
    tmp[strlen (tmp)-1] = '\0';
    if (adr = ip_stringtoaddr (tmp,&adrlen,&family)) {
      (*bn) (BLOCK_TCPOPEN,NIL);
      sock = tcp_socket_open (family,adr,adrlen,(unsigned short) port,tmp,
			      hostname = host);
      (*bn) (BLOCK_NONE,NIL);
      fs_give ((void **) &adr);
    }
    else sprintf (tmp,"Bad format domain-literal: %.80s",host);
  }

  else {			/* lookup host name */
    if (tcpdebug) {
      sprintf (tmp,"DNS resolution %.80s",host);
      mm_log (tmp,TCPDEBUG);
    }
    (*bn) (BLOCK_DNSLOOKUP,NIL);/* look up name */
    if (!(s = ip_nametoaddr (host,&adrlen,&family,&hostname,&next)))
      sprintf (tmp,"Host not found (#%d): %s",WSAGetLastError (),host);
    (*bn) (BLOCK_NONE,NIL);
    if (s) {			/* DNS resolution won? */
      if (tcpdebug) mm_log ("DNS resolution done",TCPDEBUG);
      wsa_sock_open++;		/* prevent tcp_abort() from freeing in loop */
      do {
	(*bn) (BLOCK_TCPOPEN,NIL);
	if (((sock = tcp_socket_open (family,s,adrlen,(unsigned short) port,
				      tmp,hostname)) == INVALID_SOCKET) &&
	    (s = ip_nametoaddr (NIL,&adrlen,&family,&hostname,&next)) &&
	    !silent) mm_log (tmp,WARN);
	(*bn) (BLOCK_NONE,NIL);
      } while ((sock == INVALID_SOCKET) && s);
      wsa_sock_open--;		/* undo protection */
    }
  }
  if (sock == INVALID_SOCKET) {	/* do possible cleanup action */
    if (!silent) mm_log (tmp,ERROR);
    tcp_abort (&sock);	
  }
  else {			/* got a socket, create TCP/IP stream */
    stream = (TCPSTREAM *) memset (fs_get (sizeof (TCPSTREAM)),0,
				   sizeof (TCPSTREAM));
    stream->port = port;	/* port number */
				/* init socket */
    stream->tcpsi = stream->tcpso = sock;
    stream->ictr = 0;		/* init input counter */
				/* copy official host name */
    stream->host = cpystr (hostname);
    if (tcpdebug) mm_log ("Stream open and ready for read",TCPDEBUG);
  }
  return stream;		/* return success */
}

/* Open a TCP socket
 * Accepts: protocol family
 *	    address to connect to
 *	    address length
 *	    port
 *	    scratch buffer
 *	    host name
 * Returns: socket if success, else SOCKET_ERROR with error string in scratch
 */

int tcp_socket_open (int family,void *adr,size_t adrlen,unsigned short port,
		     char *tmp,char *hst)
{
  int sock;
  size_t len;
  char *s;
  struct protoent *pt = getprotobyname ("tcp");
  struct sockaddr *sadr = ip_sockaddr (family,adr,adrlen,port,&len);
  sprintf (tmp,"Trying IP address [%s]",ip_sockaddrtostring (sadr));
  mm_log (tmp,NIL);
				/* get a TCP stream */
  if ((sock = socket (sadr->sa_family,SOCK_STREAM,pt ? pt->p_proto : 0)) ==
      INVALID_SOCKET)
    sprintf (tmp,"Unable to create TCP socket (%d)",WSAGetLastError ());
  else {
    wsa_sock_open++;		/* count this socket as open */
				/* open connection */
    if (connect (sock,sadr,len) == SOCKET_ERROR) {
      switch (WSAGetLastError ()) {
      case WSAECONNREFUSED:
	s = "Refused";
	break;
      case WSAENOBUFS:
	s = "Insufficient system resources";
	break;
      case WSAETIMEDOUT:
	s = "Timed out";
	break;
      case WSAEHOSTUNREACH:
	s = "Host unreachable";
	break;
      default:
	s = "Unknown error";
	break;
      }
      sprintf (tmp,"Can't connect to %.80s,%ld: %s (%d)",hst,port,s,
	       WSAGetLastError ());
      tcp_abort (&sock);	/* flush socket */
      sock = INVALID_SOCKET;
    }
  }
  fs_give ((void **) &sadr);
  return sock;			/* return the socket */
}
  
/* TCP/IP authenticated open
 * Accepts: NETMBX specifier
 *	    service name
 *	    returned user name buffer
 * Returns: TCP/IP stream if success else NIL
 */

TCPSTREAM *tcp_aopen (NETMBX *mb,char *service,char *usrbuf)
{
  return NIL;			/* always NIL on Windows */
}

/* TCP/IP receive line
 * Accepts: TCP/IP stream
 * Returns: text line string or NIL if failure
 */

char *tcp_getline (TCPSTREAM *stream)
{
  int n,m;
  char *st,*ret,*stp;
  char c = '\0';
  char d;
				/* make sure have data */
  if (!tcp_getdata (stream)) return NIL;
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
  if (!tcp_getdata (stream)) fs_give ((void **) &ret);
				/* special case of newline broken by buffer */
  else if ((c == '\015') && (*stream->iptr == '\012')) {
    stream->iptr++;		/* eat the line feed */
    stream->ictr--;
    ret[n - 1] = '\0';		/* tie off string with null */
  }
				/* else recurse to get remainder */
  else if (st = tcp_getline (stream)) {
    ret = (char *) fs_get (n + 1 + (m = strlen (st)));
    memcpy (ret,stp,n);		/* copy first part */
    memcpy (ret + n,st,m);	/* and second part */
    fs_give ((void **) &stp);	/* flush first part */
    fs_give ((void **) &st);	/* flush second part */
    ret[n + m] = '\0';		/* tie off string with null */
  }
  return ret;
}

/* TCP/IP receive buffer
 * Accepts: TCP/IP stream
 *	    size in bytes
 *	    buffer to read into
 * Returns: T if success, NIL otherwise
 */

long tcp_getbuffer (TCPSTREAM *stream,unsigned long size,char *s)
{
  unsigned long n;
				/* make sure socket still alive */
  if (stream->tcpsi == INVALID_SOCKET) return NIL;
				/* can transfer bytes from buffer? */
  if (n = min (size,stream->ictr)) {
    memcpy (s,stream->iptr,n);	/* yes, slurp as much as we can from it */
    s += n;			/* update pointer */
    stream->iptr +=n;
    size -= n;			/* update # of bytes to do */
    stream->ictr -=n;
  }
  if (size) {
    int i;
    fd_set fds,efds;
    struct timeval tmo;
    time_t t = time (0);
    blocknotify_t bn=(blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
    (*bn) (BLOCK_TCPREAD,NIL);
    while (size > 0) {		/* until request satisfied */
      if (tcpdebug) mm_log ("Reading TCP buffer",TCPDEBUG);
				/* simple case if not a socket */
      if (stream->tcpsi != stream->tcpso)
	while (((i = read (stream->tcpsi,s,(int) min (maxposint,size))) < 0) &&
	       (errno == EINTR));
      else {			/* socket case */
	time_t tl = time (0);
	time_t now = tl;
	int ti = ttmo_read ? now + ttmo_read : 0;
	tmo.tv_usec = 0;
	FD_ZERO (&fds);		/* initialize selection vector */
	FD_ZERO (&efds);	/* handle errors too */
				/* set bit in selection vectors */
	FD_SET (stream->tcpsi,&fds);
	FD_SET (stream->tcpsi,&efds);
	errno = NIL;		/* initially no error */
	do {			/* block under timeout */
	  tmo.tv_sec = ti ? ti - now : 0;
	  i = select (stream->tcpsi+1,&fds,NIL,&efds,ti ? &tmo : NIL);
	  now = time (0);	/* fake timeout if interrupt & time expired */
	  if ((i < 0) && ((errno = WSAGetLastError ()) == WSAEINTR) && ti &&
	      (ti <= now)) i = 0;
	} while ((i < 0) && (errno == WSAEINTR));
				/* success from select, read what we can */
	if (i > 0) while (((i = recv (stream->tcpsi,s,
				      (int) min (maxposint,size),0)) ==
			   SOCKET_ERROR) &&
			  ((errno = WSAGetLastError ()) == WSAEINTR));
	else if (!i) {		/* timeout, ignore if told to resume */
	  if (tmoh && (*tmoh) (now - t,now - tl)) continue;
				/* otherwise punt */
	  if (tcpdebug) mm_log ("TCP buffer read timeout",TCPDEBUG);
	  return tcp_abort (&stream->tcpsi);
	}
      }
      if (i <= 0) {		/* error seen? */
	if (tcpdebug) {
	  char tmp[MAILTMPLEN];
	  if (i) sprintf (s = tmp,"TCP buffer read I/O error %d",errno);
	  else s = "TCP buffer read end of file";
	  mm_log (s,TCPDEBUG);
	}
	return tcp_abort (&stream->tcpsi);
      }
      s += i;			/* point at new place to write */
      size -= i;		/* reduce byte count */
      if (tcpdebug) mm_log ("Successfully read TCP buffer",TCPDEBUG);
    }
    (*bn) (BLOCK_NONE,NIL);
  }
  *s = '\0';			/* tie off string */
  return LONGT;
}

/* TCP/IP receive data
 * Accepts: TCP/IP stream
 * Returns: T if success, NIL otherwise
 */

long tcp_getdata (TCPSTREAM *stream)
{
  int i;
  fd_set fds,efds;
  struct timeval tmo;
  time_t t = time (0);
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  if (stream->tcpsi == INVALID_SOCKET) return NIL;
  (*bn) (BLOCK_TCPREAD,NIL);
  while (stream->ictr < 1) {	/* if nothing in the buffer */
    if (tcpdebug) mm_log ("Reading TCP data",TCPDEBUG);
				/* simple case if not a socket */
    if (stream->tcpsi != stream->tcpso)
      while (((i = read (stream->tcpsi,stream->ibuf,BUFLEN)) < 0) &&
	     (errno == EINTR));
    else {
      time_t tl = time (0);
      time_t now = tl;
      int ti = ttmo_read ? now + ttmo_read : 0;
      tmo.tv_usec = 0;
      FD_ZERO (&fds);		/* initialize selection vector */
      FD_ZERO (&efds);		/* handle errors too */
				/* set bit in selection vectors */
      FD_SET (stream->tcpsi,&fds);
      FD_SET (stream->tcpsi,&efds);
      errno = NIL;		/* initially no error */
      do {			/* block under timeout */
	tmo.tv_sec = ti ? ti - now : 0;
	i = select (stream->tcpsi+1,&fds,NIL,&efds,ti ? &tmo : NIL);
	now = time (0);		/* fake timeout if interrupt & time expired */
	if ((i < 0) && ((errno = WSAGetLastError ()) == WSAEINTR) && ti &&
	    (ti <= now)) i = 0;
      } while ((i < 0) && (errno == WSAEINTR));
				/* success from select, read what we can */
      if (i > 0) while (((i = recv (stream->tcpsi,stream->ibuf,BUFLEN,0)) ==
			 SOCKET_ERROR) &&
			((errno = WSAGetLastError ()) == WSAEINTR));
      else if (!i) {		/* timeout, ignore if told to resume */
	if (tmoh && (*tmoh) (now - t,now - tl)) continue;
				/* otherwise punt */
	if (tcpdebug) mm_log ("TCP data read timeout",TCPDEBUG);
	return tcp_abort (&stream->tcpsi);
      }
    }
    if (i <= 0) {		/* error seen? */
      if (tcpdebug) {
	char *s,tmp[MAILTMPLEN];
	if (i) sprintf (s = tmp,"TCP data read I/O error %d",errno);
	else s = "TCP data read end of file";
	mm_log (tmp,TCPDEBUG);
      }
      return tcp_abort (&stream->tcpsi);
    }
    stream->iptr = stream->ibuf;/* point at TCP buffer */
    stream->ictr = i;		/* set new byte count */
    if (tcpdebug) mm_log ("Successfully read TCP data",TCPDEBUG);
  }
  (*bn) (BLOCK_NONE,NIL);
  return T;
}

/* TCP/IP send string as record
 * Accepts: TCP/IP stream
 *	    string pointer
 * Returns: T if success else NIL
 */

long tcp_soutr (TCPSTREAM *stream,char *string)
{
  return tcp_sout (stream,string,(unsigned long) strlen (string));
}


/* TCP/IP send string
 * Accepts: TCP/IP stream
 *	    string pointer
 *	    byte count
 * Returns: T if success else NIL
 */

long tcp_sout (TCPSTREAM *stream,char *string,unsigned long size)
{
  int i;
  struct timeval tmo;
  fd_set fds,efds;
  time_t t = time (0);
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  tmo.tv_sec = ttmo_write;
  tmo.tv_usec = 0;
  FD_ZERO (&fds);		/* initialize selection vector */
  if (stream->tcpso == INVALID_SOCKET) return NIL;
  (*bn) (BLOCK_TCPWRITE,NIL);
  while (size > 0) {		/* until request satisfied */
    if (tcpdebug) mm_log ("Writing to TCP",TCPDEBUG);
				/* simple case if not a socket */
    if (stream->tcpsi != stream->tcpso)
      while (((i = write (stream->tcpso,string,min (size,TCPMAXSEND))) < 0) &&
	     (errno == EINTR));
    else {
      time_t tl = time (0);	/* start of request */
      time_t now = tl;
      int ti = ttmo_write ? now + ttmo_write : 0;
      tmo.tv_usec = 0;
      FD_ZERO (&fds);		/* initialize selection vector */
      FD_ZERO (&efds);		/* handle errors too */
				/* set bit in selection vectors */
      FD_SET (stream->tcpso,&fds);
      FD_SET(stream->tcpso,&efds);
      errno = NIL;		/* block and write */
      do {			/* block under timeout */
	tmo.tv_sec = ti ? ti - now : 0;
	i = select (stream->tcpso+1,NIL,&fds,&efds,ti ? &tmo : NIL);
	now = time (0);		/* fake timeout if interrupt & time expired */
	if ((i < 0) && ((errno = WSAGetLastError ()) == WSAEINTR) && ti &&
	    (ti <= now)) i = 0;
      } while ((i < 0) && (errno == WSAEINTR));
				/* OK to send data? */
      if (i > 0) while (((i = send (stream->tcpso,string,
				    (int) min (size,TCPMAXSEND),0)) ==
			 SOCKET_ERROR) &&
			((errno = WSAGetLastError ()) == WSAEINTR));
      else if (!i) {		/* timeout, ignore if told to resume */
	if (tmoh && (*tmoh) (now - t,now - tl)) continue;
				/* otherwise punt */
	if (tcpdebug) mm_log ("TCP write timeout",TCPDEBUG);
	return tcp_abort (&stream->tcpsi);
      }
    }
    if (i <= 0) {		/* error seen? */
      if (tcpdebug) {
	char tmp[MAILTMPLEN];
	sprintf (tmp,"TCP write I/O error %d",errno);
	mm_log (tmp,TCPDEBUG);
      }
      return tcp_abort (&stream->tcpsi);
    }
    string += i;		/* how much we sent */
    size -= i;			/* count this size */
    if (tcpdebug) mm_log ("successfully wrote to TCP",TCPDEBUG);
  }
  (*bn) (BLOCK_NONE,NIL);
  return T;			/* all done */
}

/* TCP/IP close
 * Accepts: TCP/IP stream
 */

void tcp_close (TCPSTREAM *stream)
{
  tcp_abort (&stream->tcpsi);	/* nuke the socket */
				/* flush host names */
  if (stream->host) fs_give ((void **) &stream->host);
  if (stream->remotehost) fs_give ((void **) &stream->remotehost);
  if (stream->localhost) fs_give ((void **) &stream->localhost);
  fs_give ((void **) &stream);	/* flush the stream */
}


/* TCP/IP abort stream
 * Accepts: WinSock socket
 * Returns: NIL, always
 */

long tcp_abort (SOCKET *sock)
{
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
				/* something to close? */
  if (sock && (*sock != INVALID_SOCKET)) {
    (*bn) (BLOCK_TCPCLOSE,NIL);
    closesocket (*sock);	/* WinSock socket close */
    *sock = INVALID_SOCKET;
    (*bn) (BLOCK_NONE,NIL);
    wsa_sock_open--;		/* drop this socket */
  }
				/* no more open streams? */
  if (wsa_initted && !wsa_sock_open) {
    mm_log ("Winsock cleanup",NIL);
    wsa_initted = 0;		/* no more sockets, so... */
    WSACleanup ();		/* free up resources until needed */
  }
  return NIL;
}

/* TCP/IP get host name
 * Accepts: TCP/IP stream
 * Returns: host name for this stream
 */

char *tcp_host (TCPSTREAM *stream)
{
  return stream->host;		/* use tcp_remotehost() if want guarantees */
}


/* TCP/IP get remote host name
 * Accepts: TCP/IP stream
 * Returns: host name for this stream
 */

char *tcp_remotehost (TCPSTREAM *stream)
{
  if (!stream->remotehost) {
    size_t sadrlen;
    struct sockaddr *sadr = ip_newsockaddr (&sadrlen);
    stream->remotehost =	/* get socket's peer name */
      ((getpeername (stream->tcpsi,sadr,&sadrlen) == SOCKET_ERROR) ||
       (sadrlen <= 0)) ? cpystr (stream->host) : tcp_name (sadr,NIL);
    fs_give ((void **) &sadr);
  }
  return stream->remotehost;
}


/* TCP/IP return port for this stream
 * Accepts: TCP/IP stream
 * Returns: port number for this stream
 */

unsigned long tcp_port (TCPSTREAM *stream)
{
  return stream->port;		/* return port number */
}


/* TCP/IP get local host name
 * Accepts: TCP/IP stream
 * Returns: local host name
 */

char *tcp_localhost (TCPSTREAM *stream)
{
  if (!stream->localhost) {
    size_t sadrlen;
    struct sockaddr *sadr = ip_newsockaddr (&sadrlen);
    stream->localhost =		/* get socket's name */
      ((stream->port & 0xffff000) ||
       ((getsockname (stream->tcpsi,sadr,&sadrlen) == SOCKET_ERROR) ||
	(sadrlen <= 0))) ? cpystr (mylocalhost ()) : tcp_name (sadr,NIL);
    fs_give ((void **) &sadr);
  }
  return stream->localhost;	/* return local host name */
}

/* TCP/IP get client host address (server calls only)
 * Returns: client host address
 */

char *tcp_clientaddr ()
{
  if (!myClientAddr) {
    size_t sadrlen;
    struct sockaddr *sadr = ip_newsockaddr (&sadrlen);
    myClientAddr =		/* get stdin's peer name */
      ((getpeername (0,sadr,&sadrlen) == SOCKET_ERROR) || (sadrlen <= 0)) ?
      cpystr ("UNKNOWN") : ip_sockaddrtostring (sadr);
    fs_give ((void **) &sadr);
  }
  return myClientAddr;
}


/* TCP/IP get client host name (server calls only)
 * Returns: client host name
 */

char *tcp_clienthost ()
{
  if (!myClientHost) {
    size_t sadrlen;
    struct sockaddr *sadr = ip_newsockaddr (&sadrlen);
    myClientHost =		/* get stdin's peer name */
      ((getpeername (0,sadr,&sadrlen) == SOCKET_ERROR) || (sadrlen <= 0)) ?
      cpystr ("UNKNOWN") : tcp_name (sadr,T);
    fs_give ((void **) &sadr);
  }
  return myClientHost;
}

/* TCP/IP get server host address (server calls only)
 * Returns: server host address
 */

char *tcp_serveraddr ()
{
  if (!myServerAddr) {
    size_t sadrlen;
    struct sockaddr *sadr = ip_newsockaddr (&sadrlen);
    myServerAddr =		/* get stdin's peer name */
      ((getsockname (0,sadr,&sadrlen) == SOCKET_ERROR) || (sadrlen <= 0)) ?
      cpystr ("UNKNOWN") : cpystr (ip_sockaddrtostring (sadr));
    fs_give ((void **) &sadr);
  }
  return myServerAddr;
}


/* TCP/IP get server host name (server calls only)
 * Returns: server host name
 */

static long myServerPort = -1;

char *tcp_serverhost ()
{
  if (!myServerHost) {
    size_t sadrlen;
    struct sockaddr *sadr = ip_newsockaddr (&sadrlen);
    if (!wsa_initted++) {	/* init Windows Sockets */
      WSADATA wsock;
      if (WSAStartup (WINSOCK_VERSION,&wsock)) {
	wsa_initted = 0;
	return "random-pc";	/* try again later? */
      }
    }
				/* get stdin's name */
    myServerHost = ((getsockname (0,sadr,(void *) &sadrlen) == SOCKET_ERROR)||
		    (sadrlen <= 0) ||
                    ((myServerPort = ip_sockaddrtoport (sadr)) < 0)) ?
      cpystr (mylocalhost ()) : tcp_name (sadr,NIL);
    fs_give ((void **) &sadr);
  }
  return myServerHost;
}


/* TCP/IP get server port number (server calls only)
 * Returns: server port number
 */

long tcp_serverport ()
{
  if (!myServerHost) tcp_serverhost ();
  return myServerPort;
}

/* TCP/IP return canonical form of host name
 * Accepts: host name
 * Returns: canonical form of host name
 */

char *tcp_canonical (char *name)
{
  char *ret,host[MAILTMPLEN];
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
				/* look like domain literal? */
  if (name[0] == '[' && name[strlen (name) - 1] == ']') return name;
  (*bn) (BLOCK_DNSLOOKUP,NIL);
  if (tcpdebug) {
    sprintf (host,"DNS canonicalization %.80s",name);
    mm_log (host,TCPDEBUG);
  }
				/* get canonical name */
  if (!ip_nametoaddr (name,NIL,NIL,&ret,NIL)) ret = name;
  (*bn) (BLOCK_NONE,NIL);	/* alarms OK now */
  if (tcpdebug) mm_log ("DNS canonicalization done",TCPDEBUG);
  return ret;
}


/* TCP/IP return name from socket
 * Accepts: socket
 *	    verbose flag
 * Returns: cpystr name
 */

char *tcp_name (struct sockaddr *sadr,long flag)
{
  char *ret,*t,adr[MAILTMPLEN],tmp[MAILTMPLEN];
  sprintf (ret = adr,"[%.80s]",ip_sockaddrtostring (sadr));
  if (allowreversedns) {
    blocknotify_t bn = (blocknotify_t)mail_parameters(NIL,GET_BLOCKNOTIFY,NIL);
    if (tcpdebug) {
      sprintf (tmp,"Reverse DNS resolution %s",adr);
      mm_log (tmp,TCPDEBUG);
    }
    (*bn) (BLOCK_DNSLOOKUP,NIL);/* quell alarms */
				/* translate address to name */
    if (t = tcp_name_valid (ip_sockaddrtoname (sadr))) {
				/* produce verbose form if needed */
      if (flag)	sprintf (ret = tmp,"%s %s",t,adr);
      else ret = t;
    }
    (*bn) (BLOCK_NONE,NIL);	/* alarms OK now */
    if (tcpdebug) mm_log ("Reverse DNS resolution done",TCPDEBUG);
  }
  return cpystr (ret);
}

/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost (void)
{
  if (!myLocalHost) {
    char tmp[MAILTMPLEN];
    if (!wsa_initted++) {	/* init Windows Sockets */
      WSADATA wsock;
      if (WSAStartup (WINSOCK_VERSION,&wsock)) {
	wsa_initted = 0;
	return "random-pc";	/* try again later? */
      }
    }
    myLocalHost = cpystr ((gethostname (tmp,MAILTMPLEN-1) == SOCKET_ERROR) ?
			  "random-pc" : tcp_canonical (tmp));
  }
  return myLocalHost;
}


/* Validate name
 * Accepts: domain name
 * Returns: T if valid, NIL otherwise
 */

char *tcp_name_valid (char *s)
{
  int c;
  char *ret,*tail;
				/* must be non-empty and not too long */
  if ((ret = (s && *s) ? s : NIL) && (tail = ret + NETMAXHOST)) {
				/* must be alnum, dot, or hyphen */
    while ((c = *s++) && (s <= tail) &&
	   (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) ||
	    ((c >= '0') && (c <= '9')) || (c == '-') || (c == '.')));
    if (c) ret = NIL;
  }
  return ret;
}
