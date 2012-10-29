/*
 * Program:	Scan directories
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
 
/* Emulator for BSD scandir() call
 * Accepts: directory name
 *	    destination pointer of names array
 *	    selection function
 *	    comparison function
 * Returns: number of elements in the array or -1 if error
 */

int scandir (char *dirname,struct direct ***namelist,select_t select,
	     compar_t compar)
{
  struct direct *p,*d,**names;
  int nitems;
  struct stat stb;
  long nlmax;
  DIR *dirp = opendir (dirname);/* open directory and get status poop */
  if ((!dirp) || (fstat (dirp->dd_fd,&stb) < 0)) return -1;
  nlmax = stb.st_size / 24;	/* guesstimate at number of files */
  names = (struct direct **) fs_get (nlmax * sizeof (struct direct *));
  nitems = 0;			/* initially none found */
  while (d = readdir (dirp)) {	/* read directory item */
				/* matches select criterion? */
    if (select && !(*select) (d)) continue;
				/* get size of direct record for this file */
    p = (struct direct *) fs_get (DIR_SIZE (d));
    p->d_ino = d->d_ino;	/* copy the poop */
    strcpy (p->d_name,d->d_name);
    if (++nitems >= nlmax) {	/* if out of space, try bigger guesstimate */
      void *s = (void *) names;	/* stupid language */
      nlmax *= 2;		/* double it */
      fs_resize ((void **) &s,nlmax * sizeof (struct direct *));
      names = (struct direct **) s;
    }
    names[nitems - 1] = p;	/* store this file there */
  }
  closedir (dirp);		/* done with directory */
				/* sort if necessary */
  if (nitems && compar) qsort (names,nitems,sizeof (struct direct *),compar);
  *namelist = names;		/* return directory */
  return nitems;		/* and size */
}
