#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: screen.c 14077 2005-09-06 20:32:28Z hubert@u.washington.edu $";
#endif
/*----------------------------------------------------------------------

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"


   Pine and Pico are registered trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior written
   permission of the University of Washington.

   Pine, Pico, and Pilot software and its included text are Copyright
   1989-2004 by the University of Washington.

   The full text of our legal notices is contained in the file called
   CPYRIGHT, included with this distribution.


   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
       screen.c
       Some functions for screen painting
          - Painting and formatting of the key menu at bottom of screen
          - Convert message status bits to string for display
          - Painting and formatting of the titlebar line on top of the screen
          - Updating of the titlebar line for changes in message number...
  ====*/


#include "headers.h"

/*
 * Internal prototypes
 */
void  format_keymenu PROTO((struct key_menu *, bitmap_t, int));
void  savebits PROTO((bitmap_t, bitmap_t, int));
int   equalbits PROTO((bitmap_t, bitmap_t, int));
char *percentage PROTO((long, long, int));
void  output_keymenu PROTO((struct key_menu *, bitmap_t, int, int));
int   digit_count PROTO((long));
void  output_titlebar PROTO((TITLE_S *));
char  sort_letter PROTO((SortOrder));
#ifdef	MOUSE
void  print_inverted_label PROTO((int, MENUITEM *));
#endif


/* Saved key menu drawing state */
static struct {
    struct key_menu *km;
    int              row,
		     column,
                     blanked;
    bitmap_t         bitmap;
} km_state;


/*
 * Longest label that can be displayed in keymenu
 */
#define	MAX_LABEL	40
#define MAX_KEYNAME	 3
static struct key last_time_buf[12];
static int keymenu_is_dirty = 1;

void
mark_keymenu_dirty()
{
    keymenu_is_dirty = 1;
}


/*
 * Write an already formatted key_menu to the screen
 *
 * Args: km     -- key_menu structure
 *       bm     -- bitmap, 0's mean don't draw this key
 *       row    -- the row on the screen to begin on, negative values
 *                 are counted from the bottom of the screen up
 *       column -- column on the screen to begin on
 *
 * The bits in the bitmap are used from least significant to most significant,
 * not left to right.  So, if you write out the bitmap in the normal way, for
 * example,
 * bm[0] = 0x5, bm[1] = 0x8, bm[2] = 0x21, bm[3] = bm[4] = bm[5] = 0
 *   0000 0101    0000 1000    0010 0001   ...
 * means that menu item 0 (first row, first column) is set, item 1 (2nd row,
 * first column) is not set, item 2 is set, items 3-10 are not set, item 11
 * (2nd row, 6th and last column) is set.  In the second menu (the second set
 * of 12 bits) items 0-3 are unset, 4 is set, 5-8 unset, 9 set, 10-11 unset.
 * That uses up bm[0] - bm[2].
 * Just to make sure, here it is drawn out for the first set of 12 items in
 * the first keymenu (0-11)
 *    bm[0] x x x x  x x x x   bm[1] x x x x  x x x x
 *          7 6 5 4  3 2 1 0                 1110 9 8
 */
void
output_keymenu(km, bm, row, column)
struct key_menu *km;
bitmap_t	 bm;
int		 row,
		 column;
{
    register struct key *k;
    struct key          *last_time;
    int                  i, j,
			 ufk,        /* using function keys */
			 real_row,
			 max_column, /* number of columns on screen */
			 off;        /* offset into keymap */
    struct variable *vars = ps_global->vars;
    COLOR_PAIR          *lastc=NULL, *label_color=NULL, *name_color=NULL;
#ifdef	MOUSE
    char		 keystr[MAX_KEYNAME + MAX_LABEL + 2];
#endif

    off    	  = km->which * 12;
    max_column    = ps_global->ttyo->screen_cols;

    if((ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 0
       || max_column <= 0){
	keymenu_is_dirty = 1;
	return;
    }


    real_row = row > 0 ? row : ps_global->ttyo->screen_rows + row;

    if(pico_usingcolor()){
	lastc = pico_get_cur_color();
	if(lastc && VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR &&
	   pico_is_good_color(VAR_KEYLABEL_FORE_COLOR) &&
	   pico_is_good_color(VAR_KEYLABEL_BACK_COLOR)){
	    label_color = new_color_pair(VAR_KEYLABEL_FORE_COLOR,
					 VAR_KEYLABEL_BACK_COLOR);
	    if(label_color)
	      (void)pico_set_colorp(label_color, PSC_NONE);
	}

	if(label_color && VAR_KEYNAME_FORE_COLOR && VAR_KEYNAME_BACK_COLOR &&
	   pico_is_good_color(VAR_KEYNAME_FORE_COLOR) &&
	   pico_is_good_color(VAR_KEYNAME_BACK_COLOR)){
	    name_color = new_color_pair(VAR_KEYNAME_FORE_COLOR,
					VAR_KEYNAME_BACK_COLOR);
	}
    }

    if(keymenu_is_dirty){
	ClearLines(real_row, real_row+1);
	keymenu_is_dirty = 0;
	/* first time through, set up storage */
	if(!last_time_buf[0].name){
	    for(i = 0; i < 12; i++){
		last_time = &last_time_buf[i];
		last_time->name  = (char *)fs_get(MAX_KEYNAME + 1);
		last_time->label = (char *)fs_get(MAX_LABEL + 1);
	    }
	}

	for(i = 0; i < 12; i++)
	  last_time_buf[i].column = -1;
    }

    for(i = 0; i < 12; i++){
	int e;

	e = off + i;
        dprint(9, (debugfile, "%2d %-7.7s %-10.10s %d\n", i,
		   km == NULL ? "(no km)" 
		   : km->keys[e].name == NULL ? "(null)" 
		   : km->keys[e].name, 		   
		   km == NULL ? "(no km)" 
		   : km->keys[e].label == NULL ? "(null)" 
		   : km->keys[e].label, km ? km->keys[e].column : 0));
#ifdef	MOUSE
	register_key(i, NO_OP_COMMAND, "", NULL, 0, 0, 0, NULL, NULL);
#endif
    }

    ufk = F_ON(F_USE_FK, ps_global);
    dprint(9, (debugfile, "row: %d, real_row: %d, column: %d\n", row, 
               real_row, column));

    for(i = 0; i < 2; i++){
	int   c, el, empty, fkey, last_in_row, fix_start;
	short next_col;
	char  temp[MAX_SCREEN_COLS+1];
	char  this_label[MAX_LABEL+1];

	j = 6*i - 1;
	if(i == 1 && !label_color)
	  max_column--;  /* Some terminals scroll if you write in the
			    lower right hand corner. If user has a
			    label_color set we'll take our chances.
			    Otherwise, we'd get one cell of Normal. */

	/*
	 * k is the key struct we're working on
	 * c is the column number
	 * el is an index into the whole keys array
	 * Last_time_buf is ordered strangely. It goes row by row instead
	 * of down each column like km does. J is an index into it.
	 */
        for(c = 0, el = off+i, k = &km->keys[el];
	    k < &km->keys[off+12] && c < max_column;
	    k += 2, el += 2){

            if(k->column > max_column)
              break;

	    j++;
            if(ufk)
              fkey = 1 + k - &km->keys[off];

	    empty     = (!bitnset(el,bm) || !(k->name && *k->name));
	    last_time = &last_time_buf[j];
	    if(k+2 < &km->keys[off+12]){
	        last_in_row = 0;
		next_col    = last_time_buf[j+1].column;
		fix_start = (k == &km->keys[off] ||
			     k == &km->keys[off+1]) ? k->column : 0;
	    }
	    else{
		last_in_row = 1;
		fix_start = 0;
	    }

	    /*
	     * Make sure there is a space between this label and
	     * the next name. That is, we prefer a space to the
	     * extra character of the label because the space
	     * separates the commands and looks nicer.
	     */
	    if(k->label){
		strcpy(this_label, k->label);
		if(!last_in_row){
		    int trunc;

		    trunc = (k+2)->column - k->column
				     - ((k->name ? strlen(k->name) : 0) + 1);
		    this_label[max(trunc - 1, 0)] = SPACE;
		    this_label[trunc]     = '\0';
		}
	    }
	    else
	      this_label[0] = '\0';

	    if(!(k->column == last_time->column
		 && (last_in_row || (k+2)->column <= next_col)
		 && ((empty && !*last_time->label && !*last_time->name)
		     || (!empty
			 && this_label && !strcmp(this_label,last_time->label)
			 && ((k->name && !strcmp(k->name,last_time->name))
			     || ufk))))){
		if(empty){
		    /* blank out key with spaces */
		    strcpy(temp, repeat_char(
				    ((last_in_row || (k+2)->column > max_column)
					 ? max_column
					 : (k+2)->column) -
				     (fix_start
					 ? 0
					 : k->column),
					 SPACE));
		    last_time->column  = k->column;
		    *last_time->name   = '\0';
		    *last_time->label  = '\0';
		    MoveCursor(real_row + i, column +
						(fix_start ? 0 : k->column));
		    Write_to_screen(temp);
		    c = (fix_start ? 0 : k->column) + strlen(temp);
		}
		else{
		    /* make sure extra space before key name is there */
		    if(fix_start){
			strcpy(temp, repeat_char(k->column, SPACE));
			MoveCursor(real_row + i, column + 0);
			Write_to_screen(temp);
		    }

		    /* short name of the key */
		    if(ufk)
		      sprintf(temp, "F%d", fkey);
		    else
		      strncpy(temp, k->name, MAX_KEYNAME);

		    temp[MAX_KEYNAME] = '\0';
		    last_time->column = k->column;
		    strcpy(last_time->name, temp);
		    /* make sure name not too long */
#ifdef	MOUSE
		    strcpy(keystr, temp);
#endif
		    MoveCursor(real_row + i, column + k->column);
		    if(!empty){
			if(name_color)
			  (void)pico_set_colorp(name_color, PSC_NONE);
			else
			  StartInverse();
		    }

		    Write_to_screen(temp);
		    c = k->column + strlen(temp);
		    if(!empty){
			if(!name_color)
			  EndInverse();
		    }

		    /* now the space after the name and the label */
		    temp[0] = '\0';
		    if(c < max_column){
			temp[0] = SPACE;
			temp[1] = '\0';
			strncat(temp, this_label, MAX_LABEL);
			
			/* Don't run over the right hand edge */
			temp[max_column - c] = '\0';
			c += strlen(temp);
		    }
		    
#ifdef	MOUSE
		    strcat(keystr, temp);
#endif
		    /* fill out rest of this key with spaces */
		    if(c < max_column){
			if(last_in_row){
			    strcat(temp, repeat_char(max_column - c, SPACE));
			    c = max_column;
			}
			else{
			    if(c < (k+2)->column){
				strcat(temp,
				    repeat_char((k+2)->column - c, SPACE));
				c = (k+2)->column;
			    }
			}
		    }

		    strcpy(last_time->label, this_label);
		    if(label_color)
		      (void)pico_set_colorp(label_color, PSC_NONE);

		    Write_to_screen(temp);
		}
	    }
#ifdef	MOUSE
	    else if(!empty)
	      /* fill in what register_key needs from cached data */
	      sprintf(keystr, "%s %s", last_time->name, last_time->label);

	    if(!empty){
	      int len;

	      /*
	       * If label ends in space,
	       * don't register the space part of label.
	       */
	      len = strlen(keystr);
	      if(keystr[len-1] == SPACE)
		len--;

	      register_key(j, (ufk) ? PF1 + fkey - 1
				    : (k->name[0] == '^')
					? ctrl(k->name[1])
					: (!strucmp(k->name, "ret"))
					    ? ctrl('M')
					    : (!strucmp(k->name, "tab"))
						? '\t'
						: (!strucmp(k->name, "spc"))
						    ? SPACE
						    : k->name[0],
			   keystr, print_inverted_label,
			   real_row+i, k->column, len, 
			   name_color, label_color);
	    }
#endif

        }

	while(++j < 6*(i+1))
	  last_time_buf[j].column = -1;
    }

    fflush(stdout);
    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
	if(label_color)
	  free_color_pair(&label_color);
	if(name_color)
	  free_color_pair(&name_color);
    }
}


#ifdef	MOUSE
/*
 * print_inverted_label - highlight the label of the given menu item.
 * (callback from pico mouse routines)
 *
 * This stuff's a little strange because the row range is inclusive but
 * the column range excludes the right hand edge. So far, this is only
 * ever called with the top left row equal to the bottom right row.
 * If you change that you probably need to fix it.
 */
void
print_inverted_label(state, m)
    int state;
    MENUITEM *m;
{
    unsigned i, j;
    int      col_offset, do_color = 0, skip = 0, len, c;
    char    *lp, *label;
    COLOR_PAIR *name_color = NULL, *label_color = NULL, *lastc = NULL;
    COLOR_PAIR *deflabelc = NULL;
    struct variable *vars = ps_global->vars;

    col_offset = (m->label && (lp=strchr(m->label, ' '))) ? (lp - m->label) : 0;
    if(pico_usingcolor() && ((VAR_KEYLABEL_FORE_COLOR &&
			      VAR_KEYLABEL_BACK_COLOR) ||
			     (VAR_KEYNAME_FORE_COLOR &&
			      VAR_KEYNAME_BACK_COLOR))){
	lastc = pico_get_cur_color();

	if(VAR_KEYNAME_FORE_COLOR && VAR_KEYNAME_BACK_COLOR){
	    name_color = state ? new_color_pair(VAR_KEYNAME_BACK_COLOR,
						VAR_KEYNAME_FORE_COLOR)
			       : new_color_pair(VAR_KEYNAME_FORE_COLOR,
						VAR_KEYNAME_BACK_COLOR);
	}
	else if(VAR_REV_FORE_COLOR && VAR_REV_BACK_COLOR)
	  name_color = new_color_pair(VAR_REV_FORE_COLOR, VAR_REV_BACK_COLOR);

	if(VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR){
	    label_color = state ? new_color_pair(VAR_KEYLABEL_BACK_COLOR,
						 VAR_KEYLABEL_FORE_COLOR)
			        : new_color_pair(VAR_KEYLABEL_FORE_COLOR,
						 VAR_KEYLABEL_BACK_COLOR);
	}
	else if(VAR_REV_FORE_COLOR && VAR_REV_BACK_COLOR){
	    label_color = state ? new_color_pair(VAR_REV_FORE_COLOR,
						 VAR_REV_BACK_COLOR)
			        : new_color_pair(VAR_NORM_FORE_COLOR,
						 VAR_NORM_BACK_COLOR);
	}

	/*
	 * See if we can grok all these colors. If not, we're going to
	 * punt and pretend there are no colors at all.
	 */
	if(!pico_is_good_colorpair(name_color) ||
	   !pico_is_good_colorpair(label_color)){
	    if(name_color)
	      free_color_pair(&name_color);
	    if(label_color)
	      free_color_pair(&label_color);
	    if(lastc)
	      free_color_pair(&lastc);
	}
	else{
	    do_color++;
	    (void)pico_set_colorp(label_color, PSC_NONE);
	    if(!(VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR)){
		if(state)
		  StartInverse();
		else
		  EndInverse();
	    }
	}
    }

    if(!do_color){
	/*
	 * Command name's already inverted, leave it.
	 */
	skip = state ? 0 : col_offset;
	if(state)
	  StartInverse();
	else
	  EndInverse();
    }

    MoveCursor((int)(m->tl.r), (int)(m->tl.c) + skip);

    label = m->label ? m->label : "";
    len = strlen(label);
    for(i = m->tl.r; i <= m->br.r; i++)
      for(j = m->tl.c + skip; j < m->br.c; j++){
	  c = (i == m->lbl.r &&
	       j - m->lbl.c >= 0 &&	/* what's this, has to be true? */
	       j - m->lbl.c < len) ? label[j - m->lbl.c] : ' ';
	  if(name_color && col_offset && j == m->lbl.c)
	    (void)pico_set_colorp(name_color, PSC_NONE);

	  if(name_color && col_offset && j == m->lbl.c + col_offset){
	      if(label_color)
		(void)pico_set_colorp(label_color, PSC_NONE);
	      else{
		  if(state)
		    StartInverse();
		  else
		    EndInverse();
	      }
	  }

	  Writechar((unsigned int)c, 0);
      }

    if(do_color){
	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}
	else if(state)
	  EndInverse();
	else
	  pico_set_normal_color();
	
	if(name_color)
	  free_color_pair(&name_color);
	if(label_color)
	  free_color_pair(&label_color);
    }
    else{
	if(state)
	  EndInverse();
    }
}
#endif	/* MOUSE */


/*
 * Clear the key menu lines.
 */
void
blank_keymenu(row, column)
int row, column;
{
    struct variable *vars = ps_global->vars;
    COLOR_PAIR *lastc;

    if(FOOTER_ROWS(ps_global) > 1){
	km_state.blanked    = 1;
	km_state.row        = row;
	km_state.column     = column;
	MoveCursor(row, column);
	lastc = pico_set_colors(VAR_KEYLABEL_FORE_COLOR,
				VAR_KEYLABEL_BACK_COLOR, PSC_NORM|PSC_RET);

	CleartoEOLN();
	MoveCursor(row+1, column);
	CleartoEOLN();
	fflush(stdout);
	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}
    }
}


static struct key cancel_keys[] = 
     {{NULL,NULL,KS_NONE},            {"^C","Cancel",KS_NONE},
      {NULL,NULL,KS_NONE},            {NULL,NULL,KS_NONE},
      {NULL,NULL,KS_NONE},            {NULL,NULL,KS_NONE},
      {NULL,NULL,KS_NONE},            {NULL,NULL,KS_NONE},
      {NULL,NULL,KS_NONE},            {NULL,NULL,KS_NONE},
      {NULL,NULL,KS_NONE},            {NULL,NULL,KS_NONE},
      {NULL,NULL,KS_NONE},            {NULL,NULL,KS_NONE}};
INST_KEY_MENU(cancel_keymenu, cancel_keys);

void
draw_cancel_keymenu()
{
    bitmap_t   bitmap;

    setbitmap(bitmap);
    draw_keymenu(&cancel_keymenu, bitmap, ps_global->ttyo->screen_cols,
		 1-FOOTER_ROWS(ps_global), 0, FirstMenu);
}


void
clearfooter(ps)
    struct pine *ps;
{
    ClearLines(ps->ttyo->screen_rows - 3, ps->ttyo->screen_rows - 1);
    mark_keymenu_dirty();
    mark_status_unknown();
}
        

/*
 * Calculate formatting for key menu at bottom of screen
 *
 * Args:  km    -- The key_menu structure to format
 *        bm    -- Bitmap indicating which menu items should be displayed.  If
 *		   an item is NULL, that also means it shouldn't be displayed.
 *		   Sometimes the bitmap will be turned on in that case and just
 *		   rely on the NULL entry.
 *        width -- the screen width to format it at
 *
 * If already formatted for this particular screen width and the requested
 * bitmap and formatted bitmap agree, return.
 *
 * The formatting results in the column field in the key_menu being
 * filled in.  The column field is the column to start the label at, the
 * name of the key; after that is the label for the key.  The basic idea
 * is to line up the end of the names and beginning of the labels.  If
 * the name is too long and shifting it left would run into previous
 * label, then shift the whole menu right, or at least that entry if
 * things following are short enough to fit back into the regular
 * spacing.  This has to be calculated and not fixed so it can cope with
 * screen resize.
 */
void
format_keymenu(km, bm, width)
struct key_menu *km;
bitmap_t	 bm;
int		 width;
{
    int spacing[7], w[6], min_w[6], tw[6], extra[6], ufk, i, set;

    /* already formatted? */
    if(!km || (width == km->width &&
	       km->how_many <= km->formatted_hm &&
	       !memcmp(km->bitmap, bm, BM_SIZE)))
      return;

    /*
     * If we're in the initial command sequence we may be using function
     * keys instead of alphas, or vice versa, so we want to recalculate
     * the formatting next time through.
     */
    if((F_ON(F_USE_FK,ps_global) && ps_global->orig_use_fkeys) ||
        (F_OFF(F_USE_FK,ps_global) && !ps_global->orig_use_fkeys)){
	km->width        = width;
	km->formatted_hm = km->how_many;
	memcpy(km->bitmap, bm, BM_SIZE);
    }

    ufk = F_ON(F_USE_FK,ps_global);	/* ufk = "Using Function Keys" */

    /* set up "ideal" columns to start in, plus fake 7th column start */
    for(i = 0; i < 7; i++)
      spacing[i] = (i * width) / 6;

    /* Loop thru each set of 12 menus */
    for(set = 0; set < km->how_many; set++){
	int         k_top, k_bot, top_name_width, bot_name_width,
	            top_label_width, bot_label_width, done, offset, next_one;
	struct key *keytop, *keybot;

	offset = set * 12;		/* offset into keymenu */

	/*
	 * Find the required widths for each box.
	 */
	for(i = 0; i < 6; i++){
	    k_top  = offset + i*2;
	    k_bot  = k_top + 1;
	    keytop = &km->keys[k_top];
	    keybot = &km->keys[k_bot];

	    /*
	     * The width of a box is the max width of top or bottom name,
	     * plus 1 space, plus the max width of top or bottom label.
	     *
	     *     ? HelpInfo
	     *    ^C Cancel
	     *    |||||||||||  = 2 + 1 + 8 = 11
	     * 
	     * Then we adjust that by adding one space after the box to
	     * separate it from the next box. The last box doesn't need that
	     * but we may need an extra space for last box to avoid putting
	     * a character in the lower right hand cell of display.
	     * We also have a minimum label width (if screen is really narrow)
	     * of 3, so at least "Hel" and "Can" shows and the rest gets
	     * truncated off right hand side.
	     */

	    top_name_width = (keytop->name && bitnset(k_top,bm))
				     ? (ufk ? (i >= 5 ? 3 : 2)
					    : strlen(keytop->name)) : 0;
	    bot_name_width = (keybot->name && bitnset(k_bot,bm))
				     ? (ufk ? (i >= 4 ? 3 : 2)
					    : strlen(keybot->name)) : 0;
	    top_label_width = (keytop->label && bitnset(k_top,bm))
				     ? strlen(keytop->label) : 0;
	    bot_label_width = (keybot->label && bitnset(k_bot,bm))
				     ? strlen(keybot->label) : 0;
	    /*
	     * The 1 for i < 5 is the space between adjacent boxes.
	     * The last 1 or 0 when i == 5 is so that we won't try to put
	     * a character in the lower right cell of the display, since that
	     * causes a linefeed on some terminals.
	     */
	    w[i] = max(top_name_width, bot_name_width) + 1 +
		   max(top_label_width, bot_label_width) +
		   ((i < 5) ? 1
			   : ((bot_label_width >= top_label_width) ? 1 : 0));

	    /*
	     * The smallest we'll squeeze a column.
	     *
	     *    X ABCDEF we'll squeeze to   X ABC
	     *   YZ GHIJ                     YZ GHI
	     */
	    min_w[i] = max(top_name_width, bot_name_width) + 1 +
		       min(max(top_label_width, bot_label_width), 3) +
		     ((i < 5) ? 1
			      : ((bot_label_width >= top_label_width) ? 1 : 0));

	    /* init trial width */
	    tw[i] = spacing[i+1] - spacing[i];
	    extra[i] = tw[i] - w[i];	/* negative if it doesn't fit */
	}

	/*
	 * See if we can fit everything on the screen.
	 */
	done = 0;
	while(!done){
	    int smallest_extra, how_small;

	    /* Find smallest extra */
	    smallest_extra = -1;
	    how_small = 100;
	    for(i = 0; i < 6; i++){
		if(extra[i] < how_small){
		    smallest_extra = i;
		    how_small = extra[i];
		}
	    }
	    
	    if(how_small >= 0)			/* everything fits */
	      done++;
	    else{
		int take_from, how_close;

		/*
		 * Find the one that is closest to the ideal width
		 * that has some extra to spare.
		 */
		take_from = -1;
		how_close = 100;
		for(i = 0; i < 6; i++){
		    if(extra[i] > 0 &&
		       ((spacing[i+1]-spacing[i]) - tw[i]) < how_close){
			take_from = i;
			how_close = (spacing[i+1]-spacing[i]) - tw[i];
		    }
		}

		if(take_from >= 0){
		    /*
		     * Found one. Take one from take_from and add it
		     * to the smallest_extra.
		     */
		    tw[smallest_extra]++;
		    extra[smallest_extra]++;
		    tw[take_from]--;
		    extra[take_from]--;
		}
		else{
		    int used_width;

		    /*
		     * Oops. Not enough space to fit everything in.
		     * Some of the labels are truncated. Some may even be
		     * truncated past the minimum. We make sure that each
		     * field is at least its minimum size, and then we cut
		     * back those over the minimum until we can fit all the
		     * minimal names on the screen (if possible).
		     */
		    for(i = 0; i < 6; i++)
		      tw[i] = max(tw[i], min_w[i]);
		    
		    used_width = 0;
		    for(i = 0; i < 6; i++)
		      used_width += tw[i];

		    while(used_width > width && !done){
			int candidate, excess;

			/*
			 * Find the one with the most width over it's
			 * minimum width.
			 */
			candidate = -1;
			excess = -100;
			for(i = 0; i < 6; i++){
			    if(tw[i] - min_w[i] > excess){
				candidate = i;
				excess = tw[i] - min_w[i];
			    }
			}

			if(excess > 0){
			    tw[candidate]--;
			    used_width--;
			}
			else
			  done++;
		    }

		    done++;
		}
	    }
	}

	/*
	 * Assign the format we came up with to the keymenu.
	 */
	next_one = 0;
	for(i = 0; i < 6; i++){
	    k_top  = offset + i*2;
	    k_bot  = k_top + 1;
	    keytop = &km->keys[k_top];
	    keybot = &km->keys[k_bot];
	    top_name_width = (keytop->name && bitnset(k_top,bm))
				     ? (ufk ? (i >= 5 ? 3 : 2)
					    : strlen(keytop->name)) : 0;
	    bot_name_width = (keybot->name && bitnset(k_bot,bm))
				     ? (ufk ? (i >= 4 ? 3 : 2)
					    : strlen(keybot->name)) : 0;

	    if(top_name_width >= bot_name_width){
		keytop->column = next_one;
		keybot->column = next_one + (top_name_width - bot_name_width);
	    }
	    else{
		keytop->column = next_one + (bot_name_width - top_name_width);
		keybot->column = next_one;
	    }

	    next_one += tw[i];
	}
    }
}


/*
 * Draw the key menu at bottom of screen
 *
 * Args:  km     -- key_menu structure
 *        bitmap -- which fields are active
 *        width  -- the screen width to format it at
 *	  row    -- where to put it
 *	  column -- where to put it
 *        what   -- this is an enum telling us whether to display the
 *		    first menu (first set of 12 keys), or to display the same
 *		    one we displayed last time, or to display a particular
 *		    one (which), or to display the next one.
 *
 * Fields are inactive if *either* the corresponding bitmap entry is 0 *or*
 * the actual entry in the key_menu is NULL.  Therefore, it is sometimes
 * useful to just turn on all the bits in a bitmap and let the NULLs take
 * care of it.  On the other hand, the bitmap gives a convenient method
 * for turning some keys on or off dynamically or due to options.
 * Both methods are used about equally.
 *
 * Also saves the state for a possible redraw later.
 *
 * Row should usually be a negative number.  If row is 0, the menu is not
 * drawn.
 */
void
draw_keymenu(km, bitmap, width, row, column, what)
struct key_menu *km;
bitmap_t	 bitmap;
int	         width, row, column;
OtherMenu        what;
{
#ifdef _WINDOWS
    configure_menu_items (km, bitmap);
#endif
    format_keymenu(km, bitmap, width);

    /*--- save state for a possible redraw ---*/
    km_state.km         = km;
    km_state.row        = row;
    km_state.column     = column;
    memcpy(km_state.bitmap, bitmap, BM_SIZE);

    if(row == 0)
      return;

    if(km_state.blanked)
      keymenu_is_dirty = 1;

    if(what == FirstMenu || what == SecondMenu || what == ThirdMenu ||
       what == FourthMenu || what == MenuNotSet){
	if(what == FirstMenu || what == MenuNotSet)
	  km->which = 0;
	else if(what == SecondMenu)
	  km->which = 1;
	else if(what == ThirdMenu)
	  km->which = 2;
	else if(what == FourthMenu)
	  km->which = 3;
	
	if(km->which >= km->how_many)
	  km->which = 0;
    }
    else if(what == NextMenu)
      km->which = (km->which + 1) % km->how_many;
    /* else what must be SameMenu */

    output_keymenu(km, bitmap, row, column);

    km_state.blanked    = 0;
}


void
redraw_keymenu()
{
    if(km_state.blanked)
      blank_keymenu(km_state.row, km_state.column);
    else
      draw_keymenu(km_state.km, km_state.bitmap, ps_global->ttyo->screen_cols,
		   km_state.row, km_state.column, SameMenu);
}
    

/*
 * some useful macros...
 */
#define	MS_DEL			(0x01)
#define	MS_NEW			(0x02)
#define	MS_ANS			(0x04)

#define	STATUS_BITS(X)	(!(X && (X)->valid) ? 0				      \
			   : (X)->deleted ? MS_DEL			      \
			     : (X)->answered ? MS_ANS			      \
			       : (as.stream				      \
				  && (ps_global->unseen_in_view		      \
				      || (!(X)->seen			      \
					  && (!IS_NEWS(as.stream)	      \
					      || F_ON(F_FAKE_NEW_IN_NEWS,     \
						      ps_global)))))          \
				      ? MS_NEW : 0)

#define	BAR_STATUS(X)	(((X) & MS_DEL) ? "DEL"   \
			 : ((X) & MS_ANS) ? "ANS"   \
		           : (as.stream		      \
			      && (!IS_NEWS(as.stream)   \
				  || F_ON(F_FAKE_NEW_IN_NEWS, ps_global)) \
			      && ((X) & MS_NEW)) ? "NEW" : "   ")


/* this is the state we actually use */
static TITLEBAR_STATE_S as;

/*
 * Save the current titlebar state and return a structure
 * describing it.
 */
TITLEBAR_STATE_S *
save_titlebar_state()
{
    TITLEBAR_STATE_S *copy;

    copy = (TITLEBAR_STATE_S *) fs_get(sizeof(*copy));
    memset((void *) copy, 0, sizeof(*copy));
    *copy = as;				/* copy the structure */
    if(as.folder_name)	   /* copy the allocated strings in the structure */
      copy->folder_name = cpystr(as.folder_name);
    if(as.context_name)
      copy->context_name = cpystr(as.context_name);

    return(copy);
}

/*
 * Restore the titlebar state returned by save_titlebar_state.
 * Caller must free the state.
 */
void
restore_titlebar_state(state)
    TITLEBAR_STATE_S *state;
{
    if(as.folder_name)
      fs_give((void **) &(as.folder_name)); /* free malloc'd values */
    if(as.context_name)
      fs_give((void **) &(as.context_name));

    as = (*state);	/* copy back structure */

    /* as inherits the malloced strings */
    state->folder_name = NULL;
    state->context_name = NULL;
}

void
free_titlebar_state(state)
    TITLEBAR_STATE_S **state;
{
    if(state && *state){
	if((*state)->folder_name)
	  fs_give((void **) &((*state)->folder_name));
	if((*state)->context_name)
	  fs_give((void **) &((*state)->context_name));
	fs_give((void **) state);
	*state = NULL;
    }
}


/*--------
------*/
int
digit_count(n)
    long n;
{
    int i;

    return((n > 9)
	     ? (1 + (((i = digit_count(n / 10L)) == 3 || i == 7) ? i+1 : i))
	     : 1);
}


static int titlebar_is_dirty = 1;

void
mark_titlebar_dirty()
{
    titlebar_is_dirty = 1;
}

/*----------------------------------------------------------------------
      Sets up style and contents of current titlebar line

    Args: title -- The title that appears in the center of the line
          display_on_screen -- flag whether to display on screen or generate
                                string
          style  -- The format/style of the titlebar line
	  msgmap -- MSGNO_S * to selected message map
          current_pl -- The current page or line number
          total_pl   -- The total line or page count

  Set the contents of the acnhor line. It's called an acnhor line
because it's always present and titlebars the user. This accesses a
number of global variables, but doesn't change any. There are 4
different styles of the titlebar line. First three parts are put
together and then the three parts are put together adjusting the
spacing to make it look nice. Finally column numbers and lengths of
various fields are saved to make updating parts of it more efficient.

It's OK to call this with a bogus current message - it is only used
to look up status of current message 
 
Formats only change the right section (part3).
  FolderName:      "<folder>"  xx Messages
  MessageNumber:   "<folder>" message x,xxx of x,xxx XXX
  TextPercent:     line xxx of xxx  xx%
  MsgTextPercent:  "<folder>" message x,xxx of x,xxx  xx% XXX
  FileTextPercent: "<filename>" line xxx of xxx  xx%

Several strings and column numbers are saved so later updates to the status 
line for changes in message number or percentage can be done efficiently.
This code is some what complex, and might benefit from some improvements.
 ----*/

char *
set_titlebar(title, stream, cntxt, folder, msgmap, display_on_screen, style,
	     current_pl, total_pl, color)
     char        *title;
     MAILSTREAM  *stream;
     CONTEXT_S   *cntxt;
     char        *folder;
     MSGNO_S	 *msgmap;
     TitleBarType style;
     int          display_on_screen;
     long	  current_pl, total_pl;
     COLOR_PAIR  *color;
{
    struct variable *vars = ps_global->vars;
    MESSAGECACHE  *mc = NULL;
    PINETHRD_S    *thrd = NULL;
    TITLE_S       *tc;

    dprint(9, (debugfile, "set_titlebar - style: %d  current message cnt:%ld",
	       style, mn_total_cur(msgmap)));
    dprint(9, (debugfile, "  current_pl: %ld  total_pl: %ld\n", 
	       current_pl, total_pl));

    as.current_msg = (mn_get_total(msgmap) > 0L)
			 ? max(0, mn_get_cur(msgmap)) : 0L;
    as.msgmap	     = msgmap;
    as.style	     = style;
    as.title	     = title;
    as.stream	     = stream;
    as.stream_status = (!as.stream || (sp_dead_stream(as.stream)))
			 ? ClosedUp : as.stream->rdonly ? OnlyRead : Nominal;

    if(color){
	memset(&as.titlecontainer.color, 0, sizeof(as.titlecontainer.color));
	if(color->fg){
	    strncpy(as.titlecontainer.color.fg, color->fg, MAXCOLORLEN);
	    as.titlecontainer.color.fg[MAXCOLORLEN] = '\0';
	}

	if(color->bg){
	    strncpy(as.titlecontainer.color.bg, color->bg, MAXCOLORLEN);
	    as.titlecontainer.color.bg[MAXCOLORLEN] = '\0';
	}
    }
    else{
	memset(&as.titlecontainer.color, 0, sizeof(as.titlecontainer.color));
	if(VAR_TITLE_FORE_COLOR){
	    strncpy(as.titlecontainer.color.fg,
		    VAR_TITLE_FORE_COLOR, MAXCOLORLEN);
	    as.titlecontainer.color.fg[MAXCOLORLEN] = '\0';
	}

	if(VAR_TITLE_BACK_COLOR){
	    strncpy(as.titlecontainer.color.bg,
		    VAR_TITLE_BACK_COLOR, MAXCOLORLEN);
	    as.titlecontainer.color.bg[MAXCOLORLEN] = '\0';
	}
    }

    if(as.folder_name)
      fs_give((void **)&as.folder_name);

    as.folder_name = cpystr(pretty_fn(folder));

    if(as.context_name)
      fs_give((void **)&as.context_name);

    /*
     * Handle setting up the context if appropriate.
     */
    if(cntxt && context_isambig(folder) && ps_global->context_list->next
       && strucmp(as.folder_name, ps_global->inbox_name)){
	/*
	 * if there's more than one context and the current folder
	 * is in it (ambiguous name), set the context name...
	 */
	as.context_name = cpystr(cntxt->nickname
				   ? cntxt->nickname
				   : cntxt->context);
    }
    else
      as.context_name = cpystr("");

    if(as.stream && style != FolderName
       && style != ThrdIndex && as.current_msg > 0L) {
	long rawno;

	if((rawno = mn_m2raw(msgmap, as.current_msg)) > 0L
	   && rawno <= as.stream->nmsgs
	   && !((mc = mail_elt(as.stream, rawno)) && mc->valid)){
	    pine_mail_fetch_flags(as.stream, long2string(rawno), NIL);
	    mc = mail_elt(as.stream, rawno);
	}
    }
    
    if(style == ThrdIndex || style == ThrdMsgNum || style == ThrdMsgPercent){
	if(mn_get_total(msgmap) > 0L)
	  thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	
	if(thrd && thrd->top && thrd->top != thrd->rawno)
	  thrd = fetch_thread(stream, thrd->top);

	if(thrd)
	  as.current_thrd = thrd->thrdno;
    }

    switch(style) {
      case ThrdIndex:
	as.total_lines = msgmap->max_thrdno;
	break;

      case TextPercent:
      case MsgTextPercent:
      case FileTextPercent :
      case ThrdMsgPercent:
        as.total_lines = total_pl;
        as.current_line = current_pl;
				        /* fall through to set state */
      case ThrdMsgNum:
      case MessageNumber:
        as.msg_state = STATUS_BITS(mc);

      case FolderName:		        /* nothing more to do for this one */
	break;
    }

    tc = format_titlebar();
    if(display_on_screen)
      output_titlebar(tc);

    return(tc->titlebar_line);
}

void 
redraw_titlebar()
{
    output_titlebar(format_titlebar());
}


void
output_titlebar(tc)
    TITLE_S *tc;
{
    COLOR_PAIR *lastc = NULL, *newcolor;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    newcolor = tc ? &tc->color : NULL;

    if(newcolor)
      lastc = pico_set_colorp(newcolor, PSC_REV | PSC_RET);
	    
    if(tc && tc->titlebar_line)
      PutLine0(0, 0, tc->titlebar_line);

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}


/*----------------------------------------------------------------------
      Redraw or draw the top line, the title bar 

 The titlebar has Four fields:
     1) "Version" of fixed length and is always positioned two spaces 
        in from left display edge.
     2) "Location" which is fixed for each style of titlebar and
        is positioned two spaces from the right display edge
     3) "Title" which is of fixed length, and is centered if
        there's space
     4) "Folder" whose existence depends on style and which can
        have it's length adjusted (within limits) so it will
        equally share the space between 1) and 2) with the 
        "Title".  The rule for existence is that in the
        space between 1) and 2) there must be one space between
        3) and 4) AND at least 50% of 4) must be displayed.

   Returns - Formatted title bar 
 ----*/
TITLE_S *
format_titlebar()
{
    char    version[50], fold_tmp[MAXPATH], *titlebar_line,
           *loc_label, *thd_label, *ss_string, *fmt, sort[10];
    int     sc, tit_len, ver_len, loc_len, fold_len, num_len, ss_len, 
            is_context, avail, extra, l1, l2, s;
    int     so;				/* include sort indicator "[A]" */

    so = F_ON(F_SHOW_SORT, ps_global);

#define LV 2	/* space between Left edge and Version, must be >= 2 */
#define VT 3	/* space between Version and Title */
#define TF 1	/* space between Title and Folder */
#define FL 2	/* space between Folder and Location */
#define LR 2	/* space between Location and Right edge, >= 2 */
#define TOT (LV+VT+TF+FL+LR)
/* half of n but round up */
#define HRU(n) (((n) <= 0) ? 0 : (((n)%2) ? ((n)+1)/2 : (n)/2))

    titlebar_line = as.titlecontainer.titlebar_line;

    /* blank the line */
    memset((void *)titlebar_line, ' ', MAX_SCREEN_COLS * sizeof(char));
    sc = min(ps_global->ttyo->screen_cols, MAX_SCREEN_COLS);
    titlebar_line[sc] = '\0';

    /* initialize */
    as.del_column     = -1;
    as.cur_mess_col   = -1;
    as.percent_column = -1;
    as.page_column    = -1;
    is_context        = strlen(as.context_name);
    sprintf(version, "PINE %.40s", pine_version); 
    ss_string         = as.stream_status == ClosedUp ? "(CLOSED)" :
                        (as.stream_status == OnlyRead
			 && !IS_NEWS(as.stream))
                           ? "(READONLY)" : "";
    ss_len            = strlen(ss_string);

    tit_len = strlen(as.title);		/* fixed title field width   */
    ver_len = strlen(version);		/* fixed version field width */

    /* if only room for title we can get out early... */
    if(tit_len + ver_len + LV + VT + LR > sc){
	int i = max(0, sc - tit_len)/2;
	strncpy(titlebar_line + i, as.title, min(sc, tit_len));
	titlebar_is_dirty = 0;
	return(&as.titlecontainer);
    }

    /* 
     * set location field's length and value based on requested style 
     */
    if(as.style == ThrdIndex)
      loc_label = is_context ? "Thd" : "Thread";
    else
      loc_label = is_context ? "Msg" : "Message";

    if(as.style == ThrdMsgNum || as.style == ThrdMsgPercent)
      thd_label = is_context ? "Thd" : "Thread";

    loc_len   = strlen(loc_label);

    if(!mn_get_total(as.msgmap)){
	sprintf(tmp_20k_buf, "No %ss", loc_label);
	loc_len += 4;
    }
    else{
	switch(as.style){
	  case FolderName :			/* "x,xxx <loc_label>s" */
	    loc_len += digit_count(mn_get_total(as.msgmap)) + 3;
	    sprintf(tmp_20k_buf, "%s %s%s", comatose(mn_get_total(as.msgmap)),
		    loc_label, plural(mn_get_total(as.msgmap)));
	    break;
	  case MessageNumber :	       	/* "<loc_label> xxx of xxx DEL"  */
	    num_len	     = digit_count(mn_get_total(as.msgmap));
	    loc_len	    += (2 * num_len) + 9;    /* add spaces and "DEL" */
	    as.cur_mess_col  = sc - (2 * num_len) - (8+LR);
	    as.del_column    = as.cur_mess_col + num_len 
	      + digit_count(as.current_msg) + 5;
	    sprintf(tmp_20k_buf, "%s %s of %s %s", loc_label,
		    strcpy(tmp_20k_buf+1000, comatose(as.current_msg)),
		    strcpy(tmp_20k_buf+1500, comatose(mn_get_total(as.msgmap))),
		    BAR_STATUS(as.msg_state));
	    break;
	  case ThrdIndex :	       	/* "<loc_label> xxx of xxx"  */
	    num_len	     = digit_count(as.total_lines);
	    loc_len	    += (2 * num_len) + 5;	/* add spaces */
	    as.cur_mess_col  = sc - (2 * num_len) - (4+LR);
	    sprintf(tmp_20k_buf, "%s %s of %s", loc_label,
		    strcpy(tmp_20k_buf + 1000, comatose(as.current_thrd)),
		    strcpy(tmp_20k_buf + 1500, comatose(as.total_lines)));
	    break;
	  case ThrdMsgNum :	       	/* "<loc_label> xxx in Thd xxx DEL"  */
	    l1   	     = digit_count(mn_get_total(as.msgmap));
	    l2 	             = digit_count(as.current_thrd);
	    loc_len	    += l1 + l2 + 10 + strlen(thd_label);
	    as.cur_mess_col  = sc - l1 - l2 - (9+LR) - strlen(thd_label);
	    as.del_column    = as.cur_mess_col + digit_count(as.current_msg)
				  + l2 + 6 + strlen(thd_label);
	    sprintf(tmp_20k_buf, "%s %s in %s %s %s", loc_label,
		    strcpy(tmp_20k_buf + 1000, comatose(as.current_msg)),
		    thd_label,
		    strcpy(tmp_20k_buf + 1500, comatose(as.current_thrd)),
		    BAR_STATUS(as.msg_state));
	    break;
	  case MsgTextPercent :		/* "<loc_label> xxx of xxx xx% DEL" */
	    num_len	       = digit_count(mn_get_total(as.msgmap));
	    loc_len	      += (2 * num_len) + 13;
	    as.cur_mess_col    = sc - (2 * num_len) - (12+LR);
	    as.percent_column  = as.cur_mess_col + num_len 
	      + digit_count(as.current_msg) + 5;
	    as.del_column  = as.percent_column + 4;
	    sprintf(tmp_20k_buf, "%s %s of %s %s %s", loc_label, 
		    strcpy(tmp_20k_buf+1000, comatose(as.current_msg)),
		    strcpy(tmp_20k_buf+1500, comatose(mn_get_total(as.msgmap))),
		    percentage(as.current_line, as.total_lines, 1),
		    BAR_STATUS(as.msg_state));
	    break;
	  case ThrdMsgPercent :	  /* "<loc_label> xxx in Thd xxx xx% DEL"  */
	    num_len	       = digit_count(mn_get_total(as.msgmap));
	    loc_len	    += (2 * num_len) + 14 + strlen(thd_label);
	    as.cur_mess_col  = sc - (2 * num_len) - (13+LR) - strlen(thd_label);
	    as.percent_column    = as.cur_mess_col + digit_count(as.current_msg)
	      + digit_count(as.current_thrd) + 6 + strlen(thd_label);
	    as.del_column  = as.percent_column + 4;
	    sprintf(tmp_20k_buf, "%s %s in %s %s %s %s", loc_label, 
		    strcpy(tmp_20k_buf+1000, comatose(as.current_msg)),
		    thd_label,
		    strcpy(tmp_20k_buf+1500, comatose(as.current_thrd)),
		    percentage(as.current_line, as.total_lines, 1),
		    BAR_STATUS(as.msg_state));
	    break;
	  case TextPercent :
	    /* NOTE: no fold_tmp setup below for TextPercent style */
	  case FileTextPercent :	/* "Line xxx of xxx xx%    " */
	    num_len = digit_count(as.total_lines);
	    as.page_column = sc - ((12+LR) + 2*num_len);
	    loc_len        = 17 + 2*num_len;
	    sprintf(tmp_20k_buf, "Line %*ld of %*ld %s    ",
		    num_len, as.current_line, 
		    num_len, as.total_lines,
		    percentage(as.current_line, as.total_lines, 1));
	    break;
	}
    }

    /* at least the version will fit */
    strncpy(titlebar_line + LV, version, ver_len);

    titlebar_is_dirty = 0;

    /* if no room for location string, bail early? */
    if(ver_len + tit_len + loc_len + LV+VT+TF+LR > sc){
	strncpy((titlebar_line + sc) - (tit_len + LR), as.title, tit_len);
        as.del_column = as.cur_mess_col = as.percent_column
	  = as.page_column = -1;
	return(&as.titlecontainer);
    }

    /* figure folder_length and what's to be displayed */
    fold_tmp[0] = '\0';
    if(as.style == FileTextPercent || as.style == TextPercent){
	if(as.style == FileTextPercent){
	    fmt      = "File: %s%s";
	    extra    = strlen("File: ");
	    avail    = sc - (ver_len + tit_len + loc_len + TOT);
	    fold_len = strlen(as.folder_name);
	    if(fold_len + extra <= avail) 	/* all of folder fit? */
	      sprintf(fold_tmp, fmt, "", as.folder_name);
	    else if(HRU(fold_len) + extra+3 <= avail)
	      sprintf(fold_tmp, fmt, "...",
		      as.folder_name + fold_len - (avail - (extra+3)));
	}
	/* else leave folder/file name blank */
    }
    else{
	int    ct_len;
	NETMBX mb;

	avail    = sc - (ver_len + tit_len + loc_len + TOT);
	fold_len = strlen(as.folder_name);

	if(so){
	    SortOrder current_sort;
	    int       current_rev;
	    char      let;

	    current_sort = mn_get_sort(ps_global->msgmap);
	    current_rev  = mn_get_revsort(ps_global->msgmap);

	    /* turn current_sort into a letter */
	    let = sort_letter(current_sort);
	    if(let == 'A' && current_rev){
		let = 'R';
		current_rev = 0;
	    }

	    sprintf(sort, "[%s%c] ", current_rev ? "R" : "", let);
	}
	else
	  sort[0] = '\0';

	s = strlen(sort);

	if(is_context
	  && as.stream_status != ClosedUp
	  && (ct_len = strlen(as.context_name))){

	    fmt   = "%-*s<%*.*s> %s%s";
	    extra = 3;		/* length from "<" ">" and SPACE */

	    if(ct_len + fold_len + ss_len + extra + s <= avail)
	      sprintf(fold_tmp, fmt,
		      s ? (avail - (ct_len + fold_len + ss_len + extra)) : 0,
		      sort, ct_len, ct_len, as.context_name,
		      as.folder_name, ss_string);
	    else if(ct_len + fold_len + ss_len + extra <= avail)
	      sprintf(fold_tmp, fmt,
		      0, "",
		      ct_len, ct_len, as.context_name,
		      as.folder_name, ss_string);
	    else if(HRU(ct_len) + fold_len + ss_len + extra <= avail)
	      sprintf(fold_tmp, fmt,
		      0, "",
		      avail-(fold_len+ss_len+extra),
		      avail-(fold_len+ss_len+extra),
		      as.context_name,
		      as.folder_name, ss_string);
	    else if(HRU(ct_len) + HRU(fold_len) + ss_len + extra <= avail)
	      sprintf(fold_tmp, fmt, 0, "",
		      HRU(ct_len), HRU(ct_len), as.context_name,
		   as.folder_name+(fold_len-(avail-(HRU(ct_len)+ss_len+extra))),
		      ss_string);
	}
	else{
	    fmt  = "%-*sFolder: %s%s";
	    extra = strlen("Folder: ");
	    if(fold_len + ss_len + extra + s <= avail)
	      sprintf(fold_tmp, fmt,
		      s ? (avail - (fold_len + ss_len + extra)) : 0,
		      sort, as.folder_name, ss_string);
	    else{
		fmt   = "%-*s%s%s";
		if(fold_len + ss_len + s <= avail)
		  sprintf(fold_tmp, fmt,
			  s ? (avail - (fold_len + ss_len)) : 0,
			  sort, as.folder_name, ss_string);
		else if(fold_len + ss_len <= avail)
		  sprintf(fold_tmp, fmt, 0, "", as.folder_name, ss_string);
		else if(HRU(fold_len) + ss_len <= avail && fold_len/2 >= 3){
		    fmt = "...%s%s";
		    sprintf(fold_tmp, fmt,
			   as.folder_name + fold_len - (avail - (3+ss_len)),
			   ss_string);
		}
		/* else leave it out */
	    }
	}

	if(as.stream
	   && as.stream->mailbox
	   && mail_valid_net_parse(as.stream->mailbox, &mb)
	   && (mb.sslflag || mb.tlsflag))
	  titlebar_line[sc - 1] = '+';
    }
    
    /* write title, location and, optionally, the folder name */
    fold_len = strlen(fold_tmp);
    strncpy(titlebar_line + ver_len + LV+VT, as.title, tit_len);
    strncpy((titlebar_line + sc) - (loc_len + LR), tmp_20k_buf, 
	    strlen(tmp_20k_buf));
    if(fold_len)
      strncpy((titlebar_line + sc) - (loc_len + fold_len + FL+LR), fold_tmp,
	      fold_len);

    return(&as.titlecontainer);
}


char
sort_letter(sort)
    SortOrder sort;
{
    char let = 'A', *p;

    if((p = sort_name(sort)) != NULL && *p){
	while(*(p+1) && islower((unsigned char) *p))
	  p++;
	
	if(*p)
	  let = *p;
    }

    return(let);
}


/*----------------------------------------------------------------------
    Update the titlebar line if the message number changed

   Args: None, uses state setup on previous call to set_titlebar.

This is a bit messy because the length of the number displayed might 
change which repositions everything after it, so we adjust all the saved 
columns and shorten tail, the string holding the rest of the line.
  ----*/

void
update_titlebar_message()
{
    long curnum = mn_get_cur(as.msgmap), maxnum;
    long oldnum;
    PINETHRD_S *thrd = NULL;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    if(as.style == ThrdIndex){
	unsigned long rawno;

	oldnum = as.current_thrd;

	if(as.stream && (rawno=mn_m2raw(as.msgmap, mn_get_cur(as.msgmap))))
	  thrd = fetch_thread(as.stream, rawno);
	
	if(thrd && thrd->top && (thrd=fetch_thread(as.stream, thrd->top)))
	  curnum = thrd->thrdno;
    }
    else if(as.cur_mess_col >= 0){
	curnum = mn_get_cur(as.msgmap);
	oldnum = as.current_msg;
    }

    if(as.cur_mess_col >= 0 && oldnum != curnum){
	COLOR_PAIR *lastc = NULL, *titlecolor;
	int delta = digit_count(curnum) - digit_count(oldnum);

	if(as.style == ThrdIndex)
	  as.current_thrd = curnum;
	else
	  as.current_msg = curnum;

	titlecolor = &as.titlecontainer.color;

	if(titlecolor)
	  lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

	if(delta){
	    maxnum = mn_get_total(as.msgmap);
	    if(as.style == ThrdIndex)
	      maxnum = as.msgmap->max_thrdno;

	    if(as.del_column >= 0)
	      as.del_column += delta;

	    if(as.style == MsgTextPercent){
		as.percent_column += delta;
		PutLine5(0, as.cur_mess_col, "%s of %s %s %s%s",
			 strcpy(tmp_20k_buf + 1000, comatose(curnum)),
			 strcpy(tmp_20k_buf + 1500, comatose(maxnum)),
			 percentage(as.current_line, as.total_lines, 0),
			 BAR_STATUS(as.msg_state),
			 repeat_char(max(0, -delta), SPACE));
	    }
	    else if(as.style == ThrdMsgNum){
	      char *thd_label;

	      thd_label = strlen(as.context_name) ? "Thd" : "Thread";
	      PutLine5(0, as.cur_mess_col, "%s in %s %s %s%s",
		       strcpy(tmp_20k_buf + 1000, comatose(curnum)),
		       thd_label,
		       strcpy(tmp_20k_buf + 1500, comatose(as.current_thrd)),
		       BAR_STATUS(as.msg_state),
		       repeat_char(max(0, -delta), SPACE));
	    }
	    else
	      PutLine4(0, as.cur_mess_col, "%s of %s %s%s",
		       strcpy(tmp_20k_buf + 1000, comatose(curnum)),
		       strcpy(tmp_20k_buf + 1500,
			      comatose(maxnum)),
		       (as.style == ThrdIndex) ? ""
					       : BAR_STATUS(as.msg_state),
		       repeat_char(max(0, -delta), SPACE));
	}
	else
	  PutLine0(0, as.cur_mess_col, comatose(curnum));

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	fflush(stdout);
    }
}



/*----------------------------------------------------------------------
    Update titlebar line's message status field ("DEL", "NEW", etc)

  Args:  None, operates on state set during most recent set_titlebar call

  ---*/
int
update_titlebar_status()
{
    long rawno;
    MESSAGECACHE *mc;
    COLOR_PAIR *lastc = NULL, *titlecolor;
    
    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return(0);
    }

    if(!as.stream || as.current_msg <= 0L || as.del_column < 0)
      return(1);

    if(as.current_msg != mn_get_cur(as.msgmap)){
	update_titlebar_message();
	return(1);
    }

    mc = ((rawno = mn_m2raw(as.msgmap, as.current_msg)) > 0L
	  && rawno <= as.stream->nmsgs)
	    ? mail_elt(as.stream, rawno) : NULL;

    if(!(mc && mc->valid))
      return(0);			/* indeterminate */

    if(mc->deleted){			/* deleted takes precedence */
	if(as.msg_state & MS_DEL)
	  return(1);
    }
    else if(mc->answered){		/* then answered */
	if(as.msg_state & MS_ANS)
	  return(1);
    }
    else if(!mc->seen && as.stream
	    && (!IS_NEWS(as.stream)
		|| F_ON(F_FAKE_NEW_IN_NEWS, ps_global))){
	if(as.msg_state & MS_NEW)	/* then seen */
	  return(1);
    }
    else{
	if(as.msg_state == 0)		/* nothing to change */
	  return(1);
    }

    as.msg_state = STATUS_BITS(mc);

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    PutLine0(0, as.del_column, BAR_STATUS(as.msg_state));

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
    return(1);
}


/*---------------------------------------------------------------------- 
    Update the percentage shown in the titlebar line

  Args: new_line_number -- line number to calculate new percentage
   
  ----*/

void
update_titlebar_percent(new_line_number)
    long new_line_number;
{
    COLOR_PAIR *lastc = NULL, *titlecolor;

    if(as.percent_column < 0 || new_line_number == as.current_line)
      return;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    as.current_line = new_line_number;

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    PutLine0(0, as.percent_column,
	     percentage(as.current_line, as.total_lines, 0));

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}



/*---------------------------------------------------------------------- 
    Update the percentage AND line number shown in the titlebar line

  Args: new_line_number -- line number to calculate new percentage
   
  ----*/

void
update_titlebar_lpercent(new_line_number)
    long new_line_number;
{
    COLOR_PAIR *lastc = NULL, *titlecolor;

    if(as.page_column < 0 || new_line_number == as.current_line)
      return;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    as.current_line = new_line_number;


    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    sprintf(tmp_20k_buf, "%*ld of %*ld %s    ",
	    digit_count(as.total_lines), as.current_line, 
	    digit_count(as.total_lines), as.total_lines,
	    percentage(as.current_line, as.total_lines, 0));

    PutLine0(0, as.page_column, tmp_20k_buf);

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}



/*----------------------------------------------------------------------
    Return static buf containing portion of lines displayed

  Args:  part -- how much so far
	 total -- how many total

  ---*/
char *
percentage(part, total, suppress_top)
    long part, total;
    int  suppress_top;
{
    static char percent[4];

    if(total == 0L || (total <= ps_global->ttyo->screen_rows
				 - HEADER_ROWS(ps_global)
				 - FOOTER_ROWS(ps_global)))
      strcpy(percent, "ALL");
    else if(!suppress_top && part <= ps_global->ttyo->screen_rows
				      - HEADER_ROWS(ps_global)
				      - FOOTER_ROWS(ps_global))
      strcpy(percent, "TOP");
    else if(part >= total)
      strcpy(percent, "END");
    else
      sprintf(percent, "%2ld%%", (100L * part)/total);

    return(percent);
}




/*
 * end_titlebar - free resources associated with titlebar state struct
 */
void
end_titlebar()
{
    if(as.folder_name)
      fs_give((void **)&as.folder_name);

    if(as.context_name)
      fs_give((void **)&as.context_name);
}


/*
 * end_keymenu - free resources associated with keymenu display cache
 */
void
end_keymenu()
{
    int i;

    for(i = 0; i < 12; i++){
	if(last_time_buf[i].name)
	  fs_give((void **)&last_time_buf[i].name);

	if(last_time_buf[i].label)
	  fs_give((void **)&last_time_buf[i].label);
    }
}


/*----------------------------------------------------------------------
   Exported method to display status of mail check

   Args: putstr -- should be NO LONGER THAN 2 bytes

 Result: putstr displayed at upper-left-hand corner of screen
  ----*/
void
check_cue_display(putstr)
     char* putstr;
{
    COLOR_PAIR *lastc = NULL, *titlecolor;
    static char check_cue_char;
  
    if(ps_global->read_predicted && 
       (check_cue_char == putstr[0]
	|| putstr[0] == ' ' && putstr[1] == '\0'))
        return;
    else{
        if(putstr[0] == ' ')
	  check_cue_char = '\0';
	else
	  check_cue_char = putstr[0];
    }

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    PutLine0(0, 0, putstr);		/* show delay cue */
    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}
