/*
 * $Id: ebind.h 13654 2004-05-07 21:43:40Z jpf $
 *
 * Program:	Default key bindings
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
 * 1989-2004 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 *
 * NOTES:
 *
 *	This files describes the key bindings for pico and the pine 
 *      composer.  The binds are static, (i.e., no way for the user
 *      to change them) so as to keep pico/composer as simple to use
 *      as possible.  This, of course, means the number of functions is
 *      greatly reduced, but, then again, this is seen as very desirable.
 *
 *      There are very limited number of flat ctrl-key bindings left, and 
 *      most of them are slated for yet-to-be implemented functions, like
 *      invoking an alternate editor in the composer and necessary funcs
 *      for imlementing attachment handling.  We really want to avoid 
 *      going to multiple keystroke functions. -mss
 *
 */

/*	EBIND:		Initial default key to function bindings for
			MicroEMACS 3.2

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#ifndef	EBIND_H
#define	EBIND_H


/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This expains the funny location of the
 * control-X commands.
 */
KEYTAB  keytab[NBINDS] = {
	{KEY_UP,		backline},
	{KEY_DOWN,		forwline},
	{KEY_RIGHT,		forwchar},
	{KEY_LEFT,		backchar},
	{KEY_PGUP,		backpage},
	{KEY_PGDN,		forwpage},
	{KEY_HOME,		gotobol},
	{KEY_END,		gotoeol},
	{KEY_DEL,		forwdel},
#ifdef	MOUSE
	{KEY_MOUSE,		mousepress},
#endif
	{CTRL|'A',		gotobol},
	{CTRL|'B',		backchar},
	{CTRL|'C',		abort_composer},
	{CTRL|'D',		forwdel},
	{CTRL|'E',		gotoeol},
	{CTRL|'F',		forwchar},
	{CTRL|'G',		whelp},
	{CTRL|'H',		backdel},
	{CTRL|'I',		tab},
	{CTRL|'J',		fillpara},
	{CTRL|'K',		killregion},
	{CTRL|'L',		pico_refresh},
	{CTRL|'M',		newline},
	{CTRL|'N',		forwline},
	{CTRL|'O',		suspend_composer},
	{CTRL|'P',		backline},
	{CTRL|'R',		insfile},
#ifdef	SPELLER
	{CTRL|'T',		spell},
#endif	/* SPELLER */
	{CTRL|'U',		yank},
	{CTRL|'V',		forwpage},
	{CTRL|'W',		forwsearch},
	{CTRL|'X',		wquit},
	{CTRL|'Y',		backpage},
#ifdef	JOB_CONTROL
	{CTRL|'Z',		bktoshell},
#endif
	{CTRL|'@',		forwword},
	{CTRL|'^',		setmark},
	{CTRL|'_',		alt_editor},
	{0x7F,			backdel},
	{0,			NULL}
};


/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This expains the funny location of the
 * control-X commands.
 */
KEYTAB  pkeytab[NBINDS] = {
	{KEY_UP,		backline},
	{KEY_DOWN,		forwline},
	{KEY_RIGHT,		forwchar},
	{KEY_LEFT,		backchar},
	{KEY_PGUP,		backpage},
	{KEY_PGDN,		forwpage},
	{KEY_HOME,		gotobol},
	{KEY_END,		gotoeol},
	{KEY_DEL,		forwdel},
#ifdef	MOUSE
	{KEY_MOUSE,		mousepress},
#endif
	{CTRL|'A',		gotobol},
	{CTRL|'B',		backchar},
	{CTRL|'C',		showcpos},
	{CTRL|'D',		forwdel},
	{CTRL|'E',		gotoeol},
	{CTRL|'F',		forwchar},
	{CTRL|'G',		whelp},
	{CTRL|'H',		backdel},
	{CTRL|'I',		tab},
	{CTRL|'J',		fillpara},
	{CTRL|'K',		killregion},
	{CTRL|'L',		pico_refresh},
	{CTRL|'M',		newline},
	{CTRL|'N',		forwline},
	{CTRL|'O',		filewrite},
	{CTRL|'P',		backline},
	{CTRL|'R',		insfile},
#ifdef	SPELLER
	{CTRL|'T',		spell},
#endif	/* SPELLER */
	{CTRL|'U',		yank},
	{CTRL|'V',		forwpage},
	{CTRL|'W',		forwsearch},
	{CTRL|'X',		wquit},
	{CTRL|'Y',		backpage},
#ifdef	JOB_CONTROL
	{CTRL|'Z',		bktoshell},
#endif
	{CTRL|'@',		forwword},
	{CTRL|'^',		setmark},
	{0x7F,			backdel},
	{0,			NULL}
};

#endif	/* EBIND_H */
