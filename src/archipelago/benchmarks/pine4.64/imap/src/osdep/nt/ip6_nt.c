/*
 * Program:	UNIX IPv6 routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	18 December 2003
 * Last Edited:	12 October 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/*
 * There is some amazingly bad design in IPv6 sockets.
 *
 * Supposedly, the new getnameinfo() and getaddrinfo() functions create an
 * abstraction that is not dependent upon IPv4 or IPv6.  However, the
 * definition of getnameinfo() requires that the caller pass the length of
 * the sockaddr instead of deriving it from sa_family.  The man page says
 * that there's an sa_len member in the sockaddr, but actually there isn't.
 * This means that any caller to getnameinfo() and getaddrinfo() has to know
 * the size for the protocol family used by that sockaddr.
 *
 * The new sockaddr_in6 is bigger than the generic sockaddr (which is what
 * connect(), accept(), bind(), getpeername(), getsockname(), etc. expect).
 * Rather than increase the size of sockaddr, there's a new sockaddr_storage
 * which is only usable for allocating space.
 */

#define SADRLEN sizeof (struct sockaddr_storage)

#define SADR4(sadr) ((struct sockaddr_in *) sadr)
#define SADR4LEN sizeof (struct sockaddr_in)
#define SADR4ADR(sadr) SADR4 (sadr)->sin_addr
#define ADR4LEN sizeof (struct in_addr)
#define SADR4PORT(sadr) SADR4 (sadr)->sin_port

#define SADR6(sadr) ((struct sockaddr_in6 *) sadr)
#define SADR6LEN sizeof (struct sockaddr_in6)
#define SADR6ADR(sadr) SADR6 (sadr)->sin6_addr
#define ADR6LEN sizeof (struct in6_addr)
#define SADR6PORT(sadr) SADR6 (sadr)->sin6_port


/* IP abstraction layer */

char *ip_sockaddrtostring (struct sockaddr *sadr);
long ip_sockaddrtoport (struct sockaddr *sadr);
void *ip_stringtoaddr (char *text,size_t *len,int *family);
struct sockaddr *ip_newsockaddr (size_t *len);
struct sockaddr *ip_sockaddr (int family,void *adr,size_t adrlen,
			      unsigned short port,size_t *len);
char *ip_sockaddrtoname (struct sockaddr *sadr);
void *ip_nametoaddr (char *name,size_t *len,int *family,char **canonical,
		     void **next);

/* Return IP address string from socket address
 * Accepts: socket address
 * Returns: IP address as name string
 */

char *ip_sockaddrtostring (struct sockaddr *sadr)
{
  static char tmp[NI_MAXHOST];
  switch (sadr->sa_family) {
  case PF_INET:			/* IPv4 */
    if (!getnameinfo (sadr,SADR4LEN,tmp,NI_MAXHOST,NIL,NIL,NI_NUMERICHOST))
      return tmp;
    break;
  case PF_INET6:		/* IPv6 */
    if (!getnameinfo (sadr,SADR6LEN,tmp,NI_MAXHOST,NIL,NIL,NI_NUMERICHOST))
      return tmp;
    break;
  }
  return "NON-IP";
}


/* Return port from socket address
 * Accepts: socket address
 * Returns: port number or -1 if can't determine it
 */

long ip_sockaddrtoport (struct sockaddr *sadr)
{
  switch (sadr->sa_family) {
  case PF_INET:
    return ntohs (SADR4PORT (sadr));
  case PF_INET6:
    return ntohs (SADR6PORT (sadr));
  }
  return -1;
}

/* Return IP address from string
 * Accepts: name string
 *	    pointer to returned length
 *	    pointer to returned address family
 * Returns: address if valid, length and family updated, or NIL
 */

void *ip_stringtoaddr (char *text,size_t *len,int *family)

{
  char tmp[MAILTMPLEN];
  static struct addrinfo *hints;
  struct addrinfo *ai;
  void *adr = NIL;
  if (!hints) {			/* hints set up yet? */
    hints = (struct addrinfo *) /* one-time setup */
      memset (fs_get (sizeof (struct addrinfo)),0,sizeof (struct addrinfo));
    hints->ai_family = AF_UNSPEC;/* allow any address family */
    hints->ai_socktype = SOCK_STREAM;
				/* numeric name only */
    hints->ai_flags = AI_NUMERICHOST;
  }
				/* case-independent lookup */
  if (text && (strlen (text) < MAILTMPLEN) &&
      (!getaddrinfo (lcase (strcpy (tmp,text)),NIL,hints,&ai))) {
    switch (*family = ai->ai_family) {
    case AF_INET:		/* IPv4 */
      adr = fs_get (*len = ADR4LEN);
      memcpy (adr,(void *) &SADR4ADR (ai->ai_addr),*len);
      break;
    case AF_INET6:		/* IPv6 */
      adr = fs_get (*len = ADR6LEN);
      memcpy (adr,(void *) &SADR6ADR (ai->ai_addr),*len);
      break;
    }
    freeaddrinfo (ai);		/* free addrinfo */
  }
  return adr;
}

/* Create a maximum-size socket address
 * Accepts: pointer to return maximum socket address length
 * Returns: new, empty socket address of maximum size
 */

struct sockaddr *ip_newsockaddr (size_t *len)
{
  return (struct sockaddr *) memset (fs_get (SADRLEN),0,*len = SADRLEN);
}


/* Stuff a socket address
 * Accepts: address family
 *	    IPv4 address
 *	    length of address
 *	    port number
 *	    pointer to return socket address length
 * Returns: socket address
 */

struct sockaddr *ip_sockaddr (int family,void *adr,size_t adrlen,
			      unsigned short port,size_t *len)
{
  struct sockaddr *sadr = ip_newsockaddr (len);
  switch (family) {		/* build socket address based upon family */
  case AF_INET:			/* IPv4 */
    sadr->sa_family = PF_INET;
				/* copy host address */
    memcpy (&SADR4ADR (sadr),adr,adrlen);
				/* copy port number in network format */
    SADR4PORT (sadr) = htons (port);
    *len = SADR4LEN;
    break;
  case AF_INET6:		/* IPv6 */
    sadr->sa_family = PF_INET6;
				/* copy host address */
    memcpy (&SADR6ADR (sadr),adr,adrlen);
				/* copy port number in network format */
    SADR6PORT (sadr) = htons (port);
    *len = SADR6LEN;
    break;
  default:			/* non-IP?? */
    sadr->sa_family = PF_UNSPEC;
    break;
  }
  return sadr;
}

/* Return name from socket address
 * Accepts: socket address
 * Returns: canonical name for that address or NIL if none
 */

char *ip_sockaddrtoname (struct sockaddr *sadr)
{
  static char tmp[NI_MAXHOST];
  switch (sadr->sa_family) {
  case PF_INET:			/* IPv4 */
    if (!getnameinfo (sadr,SADR4LEN,tmp,NI_MAXHOST,NIL,NIL,NI_NAMEREQD))
      return tmp;
    break;
  case PF_INET6:		/* IPv6 */
    if (!getnameinfo (sadr,SADR6LEN,tmp,NI_MAXHOST,NIL,NIL,NI_NAMEREQD))
      return tmp;
    break;
  }
  return NIL;
}

/* Return address from name
 * Accepts: name or NIL to return next address
 *	    pointer to previous/returned length
 *	    pointer to previous/returned address family
 *	    pointer to previous/returned canonical name
 *	    pointer to previous/return state for next-address calls
 * Returns: address with length/family/canonical updated if needed, or NIL
 */

void *ip_nametoaddr (char *name,size_t *len,int *family,char **canonical,
		     void **next)
{
  struct addrinfo *cur = NIL;
  static struct addrinfo *hints;
  static struct addrinfo *ai = NIL;
  static char lcname[MAILTMPLEN];
  if (!hints) {			/* hints set up yet? */
    hints = (struct addrinfo *) /* one-time setup */
      memset (fs_get (sizeof (struct addrinfo)),0,sizeof (struct addrinfo));
				/* allow any address family */
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
				/* need canonical name */
    hints->ai_flags = AI_CANONNAME;
  }
  if (name) {			/* name supplied? */
    if (ai) {
      freeaddrinfo (ai);	/* free old addrinfo */
      ai = NIL;
    }
				/* case-independent lookup */
    if ((strlen (name) < MAILTMPLEN) &&
	(!getaddrinfo (lcase (strcpy (lcname,name)),NIL,hints,&ai))) {
      cur = ai;			/* current block */
      if (canonical)		/* set canonical name */
	*canonical = cur->ai_canonname ? cur->ai_canonname : lcname;
				/* remember as next block */
      if (next) *next = (void *) ai;
    }
    else {			/* error */
      cur = NIL;
      if (len) *len = 0;
      if (family) *family = 0;
      if (canonical) *canonical = NIL;
      if (next) *next = NIL;
    }
  }
				/* return next in series */
  else if (next && (cur = ((struct addrinfo *) *next)->ai_next)) {
    *next = cur;		/* set as last address */
				/* set canonical in case changed */
    if (canonical && cur->ai_canonname) *canonical = cur->ai_canonname;
  }

  if (cur) {			/* got data? */
    if (family) *family = cur->ai_family;
    switch (cur->ai_family) {
    case AF_INET:
      if (len) *len = ADR4LEN;
      return (void *) &SADR4ADR (cur->ai_addr);
    case AF_INET6:
      if (len) *len = ADR6LEN;
      return (void *) &SADR6ADR (cur->ai_addr);
    }
  }
  if (len) *len = 0;		/* error return */
  return NIL;
}
