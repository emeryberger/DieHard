/*
 * $Id: dos_gen.h 7884 1998-02-28 00:15:44Z hubert $
 *
 * Program:	Operating system dependent header - MS DOS Generic
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
 * 1989-1998 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 *
 * Notes:
 *     - This file should contain the cross section of functions useful
 *       in both DOS and Windows ports of pico.
 *
 */


#ifdef TURBOC
/*
 * big stack for turbo C
 */
extern	unsigned	_stklen = 16384;
#endif
