/*
 * Program:	UNIX IPv4 old routines
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
 * Last Edited:	26 May 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#define SADRLEN sizeof (struct sockaddr)

#define SADR4(sadr) ((struct sockaddr_in *) sadr)
#define SADR4LEN sizeof (struct sockaddr_in)
#define SADR4ADR(sadr) SADR4 (sadr)->sin_addr
#define ADR4LEN sizeof (struct in_addr)
#define SADR4PORT(sadr) SADR4 (sadr)->sin_port


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
  return (sadr->sa_family == PF_INET) ?
    inet_ntoa (SADR4ADR (sadr)) : "NON-IPv4";
}


/* Return port from socket address
 * Accepts: socket address
 * Returns: port number or -1 if can't determine it
 */

long ip_sockaddrtoport (struct sockaddr *sadr)
{
  return (sadr->sa_family == PF_INET) ? ntohs (SADR4PORT (sadr)) : -1;
}


/* Return IP address from string
 * Accepts: name string
 *	    pointer to returned length
 *	    pointer to returned address family
 * Returns: address if valid, length and family updated, or NIL
 */

void *ip_stringtoaddr (char *text,size_t *len,int *family)
{
  unsigned long adr;
  struct in_addr *ret;
				/* get address */
  if ((adr = inet_addr (text)) == -1) ret = NIL;
  else {			/* make in_addr */
    ret = (struct in_addr *) fs_get (*len = ADR4LEN);
    *family = AF_INET;		/* IPv4 */
    ret->s_addr = adr;		/* set address */
  }
  return (void *) ret;
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
 *	    length of address (always 4 in IPv4)
 *	    port number
 *	    pointer to return socket address length
 * Returns: socket address or NIL if error
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
  struct hostent *he;
  return ((sadr->sa_family == PF_INET) &&
	  (he = gethostbyaddr ((char *) &SADR4ADR (sadr),ADR4LEN,AF_INET))) ?
    (char *) he->h_name : NIL;
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
  char tmp[MAILTMPLEN];
  struct hostent *he;
  void *ret = NIL;
  if (name) {			/* first lookup? */
				/* yes, do case-independent lookup */
    if ((strlen (name) < MAILTMPLEN) &&
	(he = gethostbyname (lcase (strcpy (tmp,name))))) {
      if (len) *len = he->h_length;
      if (family) *family = he->h_addrtype;
      if (canonical) *canonical = (char *) he->h_name;
      if (next) *next = 0;
      ret = he->h_addr;		/* set result to this one and only block */
    }
    else {			/* error */
      if (len) *len = 0;
      if (family) *family = 0;
      if (canonical) *canonical = NIL;
      if (next) *next = NIL;
    }
  }
  return ret;			/* return result */
}
