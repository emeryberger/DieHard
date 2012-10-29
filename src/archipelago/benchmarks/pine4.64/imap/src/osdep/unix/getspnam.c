/*
 * Program:	64-bit getsockname()/getpeername() emulator
 *
 * Author:	Mark Crispin from code contributed by Chris Ross
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	5 November 2004
 * Last Edited:	5 November 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Jacket into getpeername()
 * Accepts: socket
 *	    pointer to socket address
 *	    void pointer to len
 * Returns: 0 if success, -1 if error
 */

int Getpeername (int s,struct sockaddr *name,size_t *namelen)
{
  int ret;
  socklen_t len = (socklen_t) *namelen;
  ret = getpeername (s,name,&len);
  *namelen = (size_t) len;
  return ret;
}


/* Jacket into getsockname()
 * Accepts: socket
 *	    pointer to socket address
 *	    void pointer to len
 * Returns: 0 if success, -1 if error
 */

int Getsockname (int s,struct sockaddr *name,size_t *namelen)
{
  int ret;
  socklen_t len = (socklen_t) *namelen;
  ret = getsockname (s,name,&len);
  *namelen = (size_t) len;
  return ret;
}


#define getpeername Getpeername
#define getsockname Getsockname
