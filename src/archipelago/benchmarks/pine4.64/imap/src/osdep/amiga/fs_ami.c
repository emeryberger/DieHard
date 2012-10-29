/*
 * Program:	Free storage management routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 August 1988
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Get a block of free storage
 * Accepts: size of desired block
 * Returns: free storage block
 */

void *fs_get (size_t size)
{
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  void *data = (*bn) (BLOCK_SENSITIVE,NIL);
  void *block = malloc (size ? size : (size_t) 1);
  if (!block) fatal ("Out of memory");
  (*bn) (BLOCK_NONSENSITIVE,data);
  return (block);
}


/* Resize a block of free storage
 * Accepts: ** pointer to current block
 *	    new size
 */

void fs_resize (void **block,size_t size)
{
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  void *data = (*bn) (BLOCK_SENSITIVE,NIL);
  if (!(*block = realloc (*block,size ? size : (size_t) 1)))
    fatal ("Can't resize memory");
  (*bn) (BLOCK_NONSENSITIVE,data);
}


/* Return a block of free storage
 * Accepts: ** pointer to free storage block
 */

void fs_give (void **block)
{
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  void *data = (*bn) (BLOCK_SENSITIVE,NIL);
  free (*block);
  *block = NIL;
  (*bn) (BLOCK_NONSENSITIVE,data);
}
