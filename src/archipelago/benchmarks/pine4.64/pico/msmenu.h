#line 2 "msmenu.h"
/*
 * $Id: msmenu.h 7883 1998-02-28 00:10:21Z hubert $
 *
 * Program:	Menu item definitions - Microsoft Windows 3.1
 *
 *
 * Thomas Unger
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: tunger@cac.washington.edu
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
 */

#ifndef MSMENU_H
#define MSMENU_H


/*
 * var in pine's key structure we'll use
 */
#define	KS_OSDATAVAR			short menuitem;
#define	KS_OSDATAGET(X)			((X)->menuitem)
#define	KS_OSDATASET(X, Y)		((X)->menuitem = (Y))

/*
 * Menu key definitions.
 * Should be same values as in resouce.h
 */
#define KS_NONE			0
#define KS_RANGESTART		150

#define KS_VIEW                     150
#define KS_EXPUNGE                  151
#define KS_ZOOM                     152
#define KS_SORT                     153
#define KS_HDRMODE                  154
#define KS_MAINMENU                 155
#define KS_FLDRLIST                 156
#define KS_FLDRINDEX                157
#define KS_COMPOSER                 158
#define KS_PREVPAGE                 159
#define KS_PREVMSG                  160
#define KS_NEXTMSG                  161
#define KS_ADDRBOOK                 162
#define KS_WHEREIS                  163
#define KS_PRINT                    164
#define KS_REPLY                    165
#define KS_FORWARD                  166
#define KS_BOUNCE                   167
#define KS_DELETE                   168
#define KS_UNDELETE                 169
#define KS_FLAG                     170
#define KS_SAVE                     171
#define KS_EXPORT                   172
#define KS_TAKEADDR                 173
#define KS_SELECT                   174
#define KS_APPLY                    175
#define KS_POSTPONE                 176
#define KS_SEND                     177
#define KS_CANCEL                   178
#define KS_ATTACH                   179
#define KS_TOADDRBOOK               180
#define KS_READFILE                 181
#define KS_JUSTIFY                  182
#define KS_ALTEDITOR                183
#define KS_GENERALHELP              184
#define KS_SCREENHELP               185
#define KS_EXIT                     186
#define KS_NEXTPAGE                 187
#define KS_SAVEFILE                 188
#define KS_CURPOSITION              189
#define KS_GOTOFLDR                 190
#define KS_JUMPTOMSG                191
#define KS_RICHHDR                  192
#define KS_EXITMODE                 193
#define KS_REVIEW		    194
#define KS_KEYMENU		    195
#define KS_SELECTCUR		    196
#define KS_UNDO			    197
#define KS_SPELLCHK		    198

#define KS_RANGEEND		198

#define KS_COUNT	    ((KS_RANGEEND - KS_RANGESTART) + 1)



#define MENU_DEFAULT	300			/* Default menu for application. */
#define MENU_COMPOSER	301			/* Menu for pine's composer. */

#endif /* MSMENU_H */
