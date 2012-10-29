#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: msdlg.c 11710 2001-07-02 22:31:28Z jpf $";
#endif
/*---------------------------------------------------------------------------
 *
 *  Module: msdlg.c
 *
 * Thomas Unger
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: tunger@cac.washington.edu
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
 *--------------------------------------------------------------------------*/

#define WIN31 
#define STRICT

#include "headers.h"


extern HWND		ghTTYWnd;
extern HINSTANCE	ghInstance;
extern BOOL		gfUseDialogs;
extern FILE		*mswin_debugfile;
extern int		mswin_debug;
extern char		gszAppName[45];



#define BTN_FIRSTID	200
#define BSPACE		4
#define BWIDTH_MIN	120


typedef struct {
    char	*prompt;		/* Prompt. */
    char	*string;		/* Resulting string. */
    int		strlen;			/* Length of buffer. */
    int		append;			/* Append to existing string. */
    int		passwd;			/* Passwd, don't echo. */
    unsigned	flags;			/* other flags. */
    int		dflt;
    int		cancel;
    int		button_count;		/* Number of additional buttons. */
    MDlgButton	*button_list;		/* List of other buttons. */
    char	**helptext;		/* Help text. */
    char	helpkey;
    int		result;
} OEInfo;


static OEInfo	gOEInfo;		
static BOOL	gDoHelpFirst;
static WNDPROC	gpOldEditProc;		/* Old Edit window proc. */
static WNDPROC  gpEditProc; 
static WNDPROC	gpOldBtnProc;		/* Old Edit window proc. */
static WNDPROC  gpBtnProc; 




/*
 * Forward function declaratins.
 */
LONG FAR PASCAL __export   EditProc(HWND hBtn, UINT msg, WPARAM wParam, 
					LPARAM lParam);
LONG FAR PASCAL __export   ButtonProc(HWND hBtn, UINT msg, WPARAM wParam, 
					LPARAM lParam);
BOOL CALLBACK  __export    mswin_dialog_proc (HWND hDlg, UINT uMsg, 
					WPARAM wParam, LPARAM lParam);
BOOL CALLBACK  __export    mswin_select_proc (HWND hDlg, UINT uMsg, 
					WPARAM wParam, LPARAM lParam);
LONG FAR PASCAL __export   KBCProc (HWND hWnd, UINT uMsg, WPARAM wParam, 
					LPARAM lParam);

				
static void		BuildButtonList (HWND hDlg, MDlgButton *built_in, 
				int biCount, MDlgButton *button_list);
static BOOL		ProcessChar (HWND hWnd, WPARAM wParam, 
				OEInfo *oeinfo, long *ret);
static void		GetBtnPos (HWND hPrnt, HWND hWnd, RECT *r);
			
			


			
			
			
int
mswin_usedialog (void)
{
    return (gfUseDialogs);
}
			
			

/*---------------------------------------------------------------------- 


  Generic text entry.  Prompt user for a string.
	  

  Args:
        prompt -- The string to prompt with
	string -- the buffer result is returned in, and original string (if 
                   any) is passed in.
        field_len -- Maximum length of string to accept
        append_current -- flag indicating string should not be truncated before
                          accepting input
        passwd -- a pass word is being fetch. Don't echo on screen
	button_list -- pointer to array of mDlgButton's.  input chars matching
                       those in list return value from list.  Nearly identical
		       to Pine's ESCKEY structure.
        help   -- Arrary of strings for help text in bottom screen lines
        flags  -- flags

  Result:  editing input string
            returns -1 unexpected errors
            returns 0  typed CR or pressed "OK"
            returns 1  typed ESC or pressed "Cancel"
            returns 3  typed ^G or PF1 (help)
            returns 4  typed ^L for a screen redraw
	   or one of the other return values defined in button_list.

  WARNING: Care is required with regard to the escape_list processing.
           The passed array is terminated with an entry that has ch = -1.
           Function key labels and key strokes need to be setup externally!
	   Traditionally, a return value of 2 is used for ^T escapes.
		   
		   
  Note About Help:  We don't get the help text on the first call.  If we
	  want help text we have to return 3.  We'll get called again
	  with help text.  To make it look good, we want to display
	  this the second time we're called.  But we don't want to
	  display the help text every time we're called with help
	  text.  To know the difference we set gDoHelpFirst when
	  exiting to request help text.  If this is set then we
	  display help.  If not, then no help.


----------------------------------------------------------------------*/
			
			
/* 
 * xxx implement flags.
 * xxx   display.d uses flags QFFILE, QDEFLT, and QBOBUF
 */

int
mswin_dialog (char *prompt, char *string, int field_len, int append_current,
		int passwd, MDlgButton *button_list, char **help, unsigned flags)
{
    DLGPROC	dlgprc;
    int		c;
    
    
    mswin_flush ();
    
    
    gOEInfo.prompt = prompt;
    gOEInfo.string = string;
    gOEInfo.strlen = field_len;
    gOEInfo.append = append_current;
    gOEInfo.passwd = passwd;
    gOEInfo.helptext = help;
    gOEInfo.helpkey = 0x07;	/* ^G */
    gOEInfo.flags = flags;
    gOEInfo.button_list = button_list;
    gOEInfo.result = 0;

    for (c = 0; button_list && button_list[c].ch != -1; ++c);
    gOEInfo.button_count = c;
    
    gpEditProc = (WNDPROC) MakeProcInstance ((FARPROC) EditProc, 
				    ghInstance);
    
    gpBtnProc = (WNDPROC) MakeProcInstance ((FARPROC) ButtonProc, 
				    ghInstance);
    
	
    dlgprc = (DLGPROC) MakeProcInstance ((FARPROC) mswin_dialog_proc, 
				    ghInstance);
    DialogBox (ghInstance, MAKEINTRESOURCE (IDD_OPTIONALYENTER), 
		    ghTTYWnd, dlgprc);
    FreeProcInstance((FARPROC) dlgprc);
    FreeProcInstance((FARPROC) gpBtnProc);
    FreeProcInstance((FARPROC) gpEditProc);
    return (gOEInfo.result);
}
		

	
	
/*
 * Dialog procedure for the generic text entry dialog box.
 */
BOOL CALLBACK  __export 
mswin_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    BOOL	ret = FALSE;
    int		i, l;
    int		biCount;
    HWND	hEdit;
    MDlgButton	built_in[3];
    
    
    switch (uMsg) {
    case WM_INITDIALOG:
	/*
	 * Set the prompt text.
	 */
	SetDlgItemText (hDlg, IDC_PROMPT, gOEInfo.prompt);

	/*
	 * Set the initial edit text and configure the edit control
	 */
	if (!gOEInfo.append)
	    *gOEInfo.string = '\0';
	SetDlgItemText (hDlg, IDC_RESPONCE, gOEInfo.string);
	SendDlgItemMessage (hDlg, IDC_RESPONCE, EM_SETPASSWORDCHAR,
		gOEInfo.passwd ? '*' : 0, 0L);
	SendDlgItemMessage (hDlg, IDC_RESPONCE, EM_LIMITTEXT, 
		gOEInfo.strlen - 1, 0L);
	l = lstrlen (gOEInfo.string);
	SendDlgItemMessage(hDlg, IDC_RESPONCE, EM_SETSEL, 0, 
		MAKELONG(l, l));
	hEdit = GetDlgItem (hDlg, IDC_RESPONCE);
	SetFocus (hEdit);
	gpOldEditProc = (WNDPROC) GetWindowLong (hEdit, GWL_WNDPROC);
	SetWindowLong (hEdit, GWL_WNDPROC, (DWORD)(FARPROC)gpEditProc);
	
	
	/*
	 * Subclass the standard buttons and build buttons for each item 
	 * in the button_list.
	 */
	gpOldBtnProc = (WNDPROC) GetWindowLong (GetDlgItem (hDlg, IDOK), GWL_WNDPROC);
	SetWindowLong (GetDlgItem (hDlg, IDOK), GWL_WNDPROC, 
					(DWORD)(FARPROC)gpBtnProc);
	SetWindowLong (GetDlgItem (hDlg, IDCANCEL), GWL_WNDPROC, 
					(DWORD)(FARPROC)gpBtnProc);

	memset (&built_in, 0, sizeof (built_in));
	built_in[0].id = IDOK;
	built_in[1].id = IDCANCEL;
	if (1) {
	    built_in[2].id = IDC_GETHELP;
	    biCount = 3;
        }
	else {
	    DestroyWindow (GetDlgItem (hDlg, IDC_GETHELP));
	    biCount = 2;
        }
	BuildButtonList (hDlg, built_in, biCount, gOEInfo.button_list);

	
	if (gOEInfo.helptext && gDoHelpFirst) {
	    mswin_showhelpmsg ((WINHAND)hDlg, gOEInfo.helptext);
	    gDoHelpFirst = FALSE;
        }
	return (0);
	

    case WM_ACTIVATE:
	/*
	 * Keep focus on the edit item so key strokes are always
	 * received.
	 */
	SetFocus (GetDlgItem (hDlg, IDC_RESPONCE));
	break;

	
    case WM_COMMAND:
	switch (wParam) {
	case IDOK:
	    /*
	     * Normal exit.  Accept the new text. 
	     */
	    GetDlgItemText (hDlg, IDC_RESPONCE, gOEInfo.string, 
		    gOEInfo.strlen);
	    gOEInfo.result = 0;
	    EndDialog (hDlg, gOEInfo.result);
	    ret = TRUE;
	    break;
	    
	case IDCANCEL:
	    /*
	     * Cancel operation.  Don't retreive new text.
	     */
	    gOEInfo.result = 1;
	    EndDialog (hDlg, gOEInfo.result);
	    ret = TRUE;
	    break;
	    
	case IDC_GETHELP:
	    /*
	     * Get help.  If we have help text display it.  If not,
	     * return value 3, which tells the caller to provide us
	     * with help text.
	     */
	    if (gOEInfo.helptext != NULL) {
		mswin_showhelpmsg ((WINHAND)hDlg, gOEInfo.helptext);
	    }
	    else {
		GetDlgItemText (hDlg, IDC_RESPONCE, gOEInfo.string, 
			gOEInfo.strlen);
		gOEInfo.result = 3;
		gDoHelpFirst = TRUE;
		EndDialog (hDlg, gOEInfo.result);
	    }
	    ret = TRUE;
	    break;
	    
	default:
	    /*
	     * Search button list for button with this ID.  If found
	     * return it's result code.  Retreive text.
	     */
	    if (  wParam >= BTN_FIRSTID && 
		  wParam < BTN_FIRSTID + gOEInfo.button_count) {
		GetDlgItemText (hDlg, IDC_RESPONCE, gOEInfo.string, 
		    gOEInfo.strlen);
		i = wParam - BTN_FIRSTID;
		gOEInfo.result = gOEInfo.button_list[i].rval;
		EndDialog (hDlg, gOEInfo.result);
		ret = TRUE;
	    }
	}
	
    }
    return (ret) ;
}




/*
 * Subclassed window procedure for Edit control on generic text
 * entry dialog box.
 */
LONG FAR PASCAL __export
EditProc (HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HWND	hDlg;
    int		i;
    int		id;
    LONG	ret;
    
    hDlg = GetParent (hEdit);
    
    if (msg == WM_GETDLGCODE) {
	/*
	 * Tell windows that we want to receive ALL keystrokes.
	 */
	ret = CallWindowProc (gpOldEditProc, hEdit, msg, wParam, lParam);
	ret |= DLGC_WANTALLKEYS;
	return (ret);
    }
    

    if (msg == WM_CHAR) {
	/*
	 * See if the character typed is a shortcut for any of the
	 * buttons.
	 */
	if (ProcessChar (hDlg, wParam, &gOEInfo, &ret)) 
	     return (ret);
        
        /* No... Fall through for deault processing. */
    }

    return (CallWindowProc (gpOldEditProc, hEdit, msg, wParam, lParam));
}




/*
 * Subclass button windows.
 *
 * These buttons will automatically return the input focus to 
 * a control with id IDC_RESPONCE.
 */
LONG FAR PASCAL __export
ButtonProc (HWND hBtn, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND	hDlg;
    int		i;
    int		id;
    LONG	ret;
    

    if (uMsg == WM_LBUTTONUP || uMsg == WM_MBUTTONUP || uMsg == WM_RBUTTONUP) {
	/*
	 * On mouse button up restore input focus to IDC_RESPONCE, which
	 * processes keyboard input.
	 */
	ret = CallWindowProc (gpOldBtnProc, hBtn, uMsg, wParam, lParam);
	hDlg = GetParent (hBtn);
	SetFocus (GetDlgItem (hDlg, IDC_RESPONCE));
	return (ret);
    }
    
    return (CallWindowProc (gpOldBtnProc, hBtn, uMsg, wParam, lParam));
}




/*
 * Ask a yes/no question with the MessageBox procedure.
 *
 * Returns:
 *	0	- Cancel operation.
 *	1	- "Yes" was selected.
 *	2	- "No" was selected.
 */
int
mswin_yesno (char *prompt) 
{
    int		ret;
    
    mswin_flush ();
    
    mswin_killsplash();

    ret = MessageBox (ghTTYWnd, prompt, gszAppName, 
		MB_APPLMODAL | MB_ICONQUESTION | MB_YESNOCANCEL);
	
    switch (ret) {
    case IDYES:				ret = 1;	break;
    case IDNO:				ret = 2;	break;
    default:
    case IDCANCEL:			ret = 0;	break;
    }
    return (ret);
}
    
    

	
    
    





/*---------------------------------------------------------------------- 


  Generic selection routine.  Display a prompt and a set of buttons for
  each possible answer.
	  
	  

  Args:
        prompt		-- The string to prompt with.
	button_list	-- pointer to array of mDlgButton's.  input chars 
			   matching those in list return value from
			   mlist.  Nearly identical to Pine's ESCKEY
			   mstructure.
	dflt		-- Value returned when "Enter" is pressed.
        on_ctrl_C	-- Value returned to cancel dialog (ESC).
        help		-- Arrary of strings for help text in bottom screen 
			   lines
        flags		-- flags

  Result:  
	dflt		-- Default option selected.
	on_ctrl_C	-- Calcel operation.
	or one of the other return values defined in button_list.
		
		
Note:  To prcess keyboard in put we use a custom dialog control
       which is invisible but always has the input focus.

----------------------------------------------------------------------*/

int
mswin_select (char *prompt, MDlgButton *button_list, int dflt, int on_ctrl_C, 
			char **help, unsigned flags)
{
    WNDCLASS	wndclass;
    DLGPROC	dlgprc;
    WNDPROC	kbcProc;
    int		c;

    
    mswin_flush ();
    
    
    /*
     * Setup dialog config structure.
     */
    gOEInfo.prompt = prompt;
    gOEInfo.string = NULL;
    gOEInfo.strlen = 0;
    gOEInfo.append = 0;
    gOEInfo.passwd = 0;
    gOEInfo.helptext = help;
    gOEInfo.helpkey = 'g';
    gOEInfo.dflt = dflt;
    gOEInfo.cancel = on_ctrl_C;
    gOEInfo.flags = flags;
    gOEInfo.button_list = button_list;
    gOEInfo.result = 0;

    /*
     * Count the buttons.
     */
    for (c = 0; button_list && button_list[c].ch != -1; ++c);
    gOEInfo.button_count = c;

    
    /*
     * Register the class for the user control which will receive keyboard
     * input events.
     */
    kbcProc = (WNDPROC) MakeProcInstance ((FARPROC) KBCProc, ghInstance);
    wndclass.style = 0;
    wndclass.lpfnWndProc = kbcProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = ghInstance;
    wndclass.hIcon = NULL;
    wndclass.hCursor = LoadCursor (NULL, IDC_ARROW);
    wndclass.hbrBackground = 0;
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = "KeyboardCapture";

    if (RegisterClass (&wndclass) == 0) {
	/*
	 * There is no good return value to indicate an error.  
	 * Cancel the operation.
	 */
	return (on_ctrl_C);
    }
    



    /*
     * Make other procedure instances.
     */
    dlgprc = (DLGPROC) MakeProcInstance ((FARPROC) mswin_select_proc, 
				    ghInstance);
    gpBtnProc = (WNDPROC) MakeProcInstance ((FARPROC) ButtonProc, 
				    ghInstance);

			    
    /*
     * Bring up the dialog box.
     */
    DialogBox(ghInstance, MAKEINTRESOURCE (IDD_SELECT), ghTTYWnd, 
			    dlgprc);
		    
		    

    /*
     * Free the proc instances and window class.
     */
    FreeProcInstance((FARPROC) dlgprc);
    FreeProcInstance((FARPROC) gpBtnProc);
    UnregisterClass ("KeyboardCapture", ghInstance);
    FreeProcInstance((FARPROC) kbcProc);
    return (gOEInfo.result);
}
		

	
	
/*
 * Dialog procedure for the button selection dialog box.
 */
BOOL CALLBACK  __export 
mswin_select_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    BOOL	ret = FALSE;
    int		i, l;
    int		biCount;
    HWND	hEdit;
    MDlgButton	built_in[3];
    
    if (mswin_debug >= 1)
	fprintf (mswin_debugfile, "Select:  uMsg x%x (%d), wParam x%x, lParam x%lx\n",
		uMsg, uMsg, wParam, lParam);
    
    
    switch (uMsg) {
    case WM_INITDIALOG:
	/*
	 * Show the prompt.
	 */
	SetDlgItemText (hDlg, IDC_PROMPT, gOEInfo.prompt);

	/*
	 * Set focus to the invisible custom control
	 */
	SetFocus (GetDlgItem (hDlg, IDC_RESPONCE));
	
	/*
	 * Subclass the standard buttons and build buttons for each item 
	 * in the button_list.
	 */
	gpOldBtnProc = (WNDPROC) GetWindowLong (GetDlgItem (hDlg, IDCANCEL), GWL_WNDPROC);
	SetWindowLong (GetDlgItem (hDlg, IDC_GETHELP), GWL_WNDPROC, 
					(DWORD)(FARPROC)gpBtnProc);
	SetWindowLong (GetDlgItem (hDlg, IDCANCEL), GWL_WNDPROC, 
					(DWORD)(FARPROC)gpBtnProc);

	memset (&built_in, 0, sizeof (built_in));
	built_in[0].id = IDCANCEL;
	if (gOEInfo.helptext != NULL) {
	    built_in[1].id = IDC_GETHELP;
	    biCount = 2;
        }
	else {
	    DestroyWindow (GetDlgItem (hDlg, IDC_GETHELP));
	    gOEInfo.helpkey = 0;
	    biCount = 1;
        }
	BuildButtonList (hDlg, built_in, biCount, gOEInfo.button_list);
	gDoHelpFirst = FALSE;
	return (0);
	
	
    case WM_ACTIVATE:
	/*
	 * Keep focus on the custom control item so key strokes are always
	 * received.
	 */
	SetFocus (GetDlgItem (hDlg, IDC_RESPONCE));
	break;

	
    case WM_COMMAND:
	switch (wParam) {
	case IDOK:
	    gOEInfo.result = gOEInfo.dflt;
	    EndDialog (hDlg, gOEInfo.result);
	    ret = TRUE;
	    break;
	    
	case IDCANCEL:
	    gOEInfo.result = gOEInfo.cancel;
	    EndDialog (hDlg, gOEInfo.result);
	    ret = TRUE;
	    break;

	case IDC_GETHELP:
	    /*
	     * Get help.  If we have help text display it.  If not,
	     * return value 3, which tells the caller to provide us
	     * with help text.
	     */
	    if (gOEInfo.helptext != NULL) {
		mswin_showhelpmsg ((WINHAND) hDlg, gOEInfo.helptext);
	    }
	    ret = TRUE;
	    break;
	    
	default:
	    if (  wParam >= BTN_FIRSTID && 
		  wParam < BTN_FIRSTID + gOEInfo.button_count) {
		i = wParam - BTN_FIRSTID;
		gOEInfo.result = gOEInfo.button_list[i].rval;
		EndDialog (hDlg, gOEInfo.result);
		ret = TRUE;
	    }
	}
	
    }
    return (ret) ;
}



/*
 * Window procedure for the hidden custom control.
 */
LONG FAR PASCAL __export
KBCProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int		i;
    LONG	ret;
    

    if (uMsg == WM_GETDLGCODE) {
	/*
	 * Tell windows that we want all the character messages.
	 */
	ret = DefWindowProc (hWnd, uMsg, wParam, lParam);
	ret |= DLGC_WANTALLKEYS;
	return (ret);
    }

    
    if (uMsg == WM_CHAR) {
	/*
	 * See if the character typed is a shortcut for any of the
	 * buttons.
	 */
	if (ProcessChar (GetParent (hWnd), wParam, &gOEInfo, &ret)) 
	     return (ret);

	/* Fall through for default processing. */
    }
    
    return (DefWindowProc (hWnd, uMsg, wParam, lParam));
}






/*
 * Check if character activates any button.  If it does, send WM_COMMAND
 * to target window.
 *
 * Args:
 *	hWnd	- Window to send message to.
 *	wParam	- character typed.
 *	oeinfo	- dialog config structure.  Contains button list.
 *	ret	- value to return to windows.
 *
 * Returns:
 *	TRUE	- Found a match.  Exit from Window Procedure returning
 *		  *ret to windows.
 *
 *	FALSE	- No match.  Continue with default processing of character
 *		  message.
 *
 */
static BOOL
ProcessChar (HWND hWnd, WPARAM wParam, OEInfo *oeinfo, long *ret)
{
    int		i;
    int		id;
    
	
    *ret = 0;
    if (wParam == 0x0d) {
	/*
	 * "Enter" is same as clicking on OK button.
	 */
	PostMessage (hWnd, WM_COMMAND, IDOK,
		    MAKELONG (GetDlgItem (hWnd, IDOK), 1));
	return (TRUE);
    }
    if (wParam == 0x1b || wParam == 0x03) {
	/*
	 * Esc is same as clicking on Cancel button.
	 */
	PostMessage (hWnd, WM_COMMAND, IDCANCEL,
		    MAKELONG (GetDlgItem (hWnd, IDCANCEL), 1));
	return (TRUE);
    }

    /*
     * Any of the custom buttons respond to this key?
     */
    for (i = 0; i < oeinfo->button_count; ++i) {
	if (wParam == oeinfo->button_list[i].ch) {
	    id = oeinfo->button_list[i].id;
	    PostMessage (hWnd, WM_COMMAND, id,
		    MAKELONG (GetDlgItem (hWnd, id), 1));
	    return (TRUE);
	}
    }
    
    /*
     * Lastly, is it the help key?
     */
    if (oeinfo->helpkey != 0 && wParam == oeinfo->helpkey) {
	id = IDC_GETHELP;
	PostMessage (hWnd, WM_COMMAND, id,
		MAKELONG (GetDlgItem (hWnd, id), 1));
	return (TRUE);
    }

    
    /*
     * Nothing we understand.
     */
    return (FALSE);
}





/*
 * Get a button position in the parent's coordinate space.
 */
static void
GetBtnPos (HWND hPrnt, HWND hWnd, RECT *r)
{
    GetWindowRect (hWnd, r);
    ScreenToClient (hPrnt, (POINT *) r);
    ScreenToClient (hPrnt, (POINT *) &r->right);
}




/*
 * Add buttons to the dialog box.
 */
static void
BuildButtonList (HWND hDlg, MDlgButton *built_in, int biCount, 
			MDlgButton *button_list)
{
    RECT	rDlg, rb1, rb2, r;
    HDC		hdc;
    MDlgButton	*pB;				/* pointer to button struct*/
    HWND	hBtn, hBtn1, hBtn2;		/* handle to buttons */
    int		btnCount;			/* extra button count */
    int		rows, cols;			/* button rows and columns */
    int		bpos;				/* button position */
    int		i;
    int		maxstrIdx, maxstrLen;		/* button w/ longest caption*/
    DWORD	textExtent;			/* width of button caption */
    int		margine;			/* left and right margines */
    int		btop, bleft;			/* button position */
    int		bwidth, bheight, w;		/* button size */
    int		bMinWidth;			/* minimum buttonwidth */
    int		bvertSpace, bhorzSpace;		/* button spacing */
    int		newWHeight, delta;		/* window resizing */
    char	caption[128];
    HFONT	btnFont;
   
    
    /*
     * Are there any buttons to add?
     */
    if (button_list == NULL)
	return;
    maxstrIdx = 0;
    for (btnCount = 0; button_list[btnCount].ch != -1; ++btnCount) {
	if (lstrlen (button_list[btnCount].label) > 
		lstrlen (button_list[maxstrIdx].label)) 
	    maxstrIdx = btnCount;
    }
    if (btnCount == 0)
	return;


    /* 
     * Get the size of the dialog box and the positions of the
     * first and, if there is one, the second button.  Calculate or
     * default button offsets and positions.
     */
    GetClientRect (hDlg, &rDlg);
    hBtn1 = GetDlgItem (hDlg, built_in[0].id);
    GetBtnPos (hDlg, hBtn1, &rb1);
    margine = rb1.left;				/* left and right margine */
    bheight = rb1.bottom - rb1.top;		/* button width */
    bwidth  = rb1.right - rb1.left;		/* button height. */
    if (biCount > 1) {
	hBtn2 = GetDlgItem (hDlg, built_in[1].id);
	GetBtnPos (hDlg, hBtn2, &rb2);
	bvertSpace = rb2.top - rb1.top;		/* vertical spacing */
    }
    else {
	bvertSpace = bheight + BSPACE;		/* vertical spacing */
    }
    
    
    /*
     * Get the button font.
     */
    btnFont = (HFONT) SendMessage (hBtn1, WM_GETFONT, 0, 0);
	

    
    /*
     * Get the screen extent of the longest button label.  min width
     * is the extent, plus the average width of 5 extra characters for
     * key stroke, plus margine.
     */
    hdc = GetDC (hBtn1);
    wsprintf (caption, "%s '%s'", button_list[maxstrIdx].label, 
				  button_list[maxstrIdx].name);
    maxstrLen = lstrlen (caption);
#ifdef	WIN32
    {
	SIZE size;

	GetTextExtentPoint32 (hdc, caption, maxstrLen, &size);
	textExtent = size.cx;
    }
#else
    textExtent = GetTextExtent (hdc, caption, maxstrLen);
    textExtent = LOWORD (textExtent);
#endif
    ReleaseDC (hBtn1, hdc);
    bMinWidth = LOWORD (textExtent) + (2 * (LOWORD(textExtent) / maxstrLen));
    if (bwidth < bMinWidth)
	bwidth = bMinWidth;

    
    
    /*
     * Calculate button positions.  If the buttons are going to extend
     * past the right edge of the dialog box, shrink their width.  But
     * if they get narrower than the min width calculated above then
     * increase the number of rows.
     */
    rows = 1;
    do {
	++rows;					/* More rows. */
	w = bwidth;				/* Original button width. */
	cols = 1 + ((biCount + btnCount) - 1) / rows;	/* Calc num cols. */

	/* Need to srink button width? */
	if (2 * margine + bwidth * cols + BSPACE * (cols - 1) > rDlg.right) {
	    w = ((rDlg.right - (2 * margine)) - (BSPACE * (cols-1))) / cols;
	}
	/* Did buttons get too narrow? */
    } while (w < bMinWidth && cols > 1);

    bwidth = w;
    bhorzSpace = bwidth + BSPACE;		/* horizontal spacing */
    
    
    


    /* 
     * Resize window. 
     */
    newWHeight = rb1.top + (rows * bvertSpace);
    delta = newWHeight - rDlg.bottom;
    if (delta != 0) {
	GetWindowRect (hDlg, &r);
	MoveWindow (hDlg, r.left, r.top, r.right - r.left, 
		(r.bottom - r.top) + delta, FALSE);
    }
	


    /*
     * Create new buttons.
     */
    for (bpos = 0; bpos < biCount + btnCount; ++bpos) {
	/*
	 * Calculate position for this button.
	 */
	btop  = rb1.top  + (bpos % rows) * bvertSpace;
	bleft = rb1.left + (bpos / rows) * bhorzSpace;
	
	if (bpos < biCount) {
	    /* 
	     * Resize existing buttons. 
	     */
	    MoveWindow (GetDlgItem (hDlg, built_in[bpos].id), 
		    bleft, btop, bwidth, bheight, FALSE);
	}
	else {
	    i = bpos - biCount;
	    pB = &button_list[i];

	    wsprintf (caption, "%s '%s'", pB->label, pB->name);
	    hBtn = CreateWindow ("BUTTON", caption, 
		    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
		    bleft, btop, bwidth, bheight, 
		    hDlg, NULL, ghInstance, NULL);

	    pB->id = BTN_FIRSTID + i;
#ifdef	WIN32
	    SetWindowLong (hBtn, GWL_ID, pB->id);
#else
	    SetWindowWord (hBtn, GWW_ID, pB->id);
#endif
	    SendMessage (hBtn, WM_SETFONT, (WPARAM)btnFont, MAKELPARAM (0, 0));

	    /* Subclass button. */
	    SetWindowLong (hBtn, GWL_WNDPROC, (DWORD)(FARPROC)gpBtnProc);
	    EnableWindow (hBtn, TRUE);
	}
    }
}
    
    
    
    
	    
