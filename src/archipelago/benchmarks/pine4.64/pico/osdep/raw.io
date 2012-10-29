/*
 * TTY setup routines. These are the TERMIO-style (System V) routines.
 */

static struct termio _raw_tty, _original_tty;

/*
 * current raw state
 */
static short _inraw = 0;

/*
 *  Set up the tty driver
 *
 * Args: state -- which state to put it in. 1 means go into raw (cbreak),
 *                0 out of raw.
 *
 * Result: returns 0 if successful and -1 if not.
 */
int
Raw(state)
int state;
{
    /** state is either ON or OFF, as indicated by call **/
    /* Check return code only on first call. If it fails we're done for and
       if it goes OK the others will probably go OK too. */

    if(state == 0 && _inraw){
        /*----- restore state to original -----*/
        if(ioctl(STDIN_FD, TCSETAW, &_original_tty) < 0)
          return(-1);

        _inraw = 0;
    }
    else if(state == 1 && ! _inraw){
        /*----- Go into raw mode (cbreak actually) ----*/
        if(ioctl(STDIN_FD, TCGETA, &_original_tty) < 0)
          return(-1);

	(void)ioctl(STDIN_FD, TCGETA, &_raw_tty);    /** again! **/
	_raw_tty.c_lflag &= ~(ICANON | ECHO);	/* noecho raw mode  */
 	_raw_tty.c_lflag &= ~ISIG;		/* disable signals */
 	_raw_tty.c_iflag &= ~ICRNL;		/* turn off CR->NL on input  */
 	_raw_tty.c_oflag &= ~ONLCR;		/* turn off NL->CR on output */
	_raw_tty.c_cc[VMIN]  = 1;		/* min # of chars to queue   */
	_raw_tty.c_cc[VTIME] = 0;		/* min time to wait for input*/
	_raw_tty.c_cc[VINTR] = ctrl('C');	/* make it our special char */
	_raw_tty.c_cc[VQUIT] = 0;
	(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	_inraw = 1;
    }

    return(0);
}


/*
 *  Set up the tty driver to use XON/XOFF flow control
 *
 * Args: state -- True to make sure XON/XOFF turned on, FALSE off.
 *
 * Result: none.
 */
void
xonxoff_proc(state)
int state;
{
    if(_inraw){
	if(state){
	    if(!(_raw_tty.c_iflag & IXON)){
		_raw_tty.c_iflag |= IXON;	/* turn ON ^S/^Q on input   */
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }
	}
	else{
	    if(_raw_tty.c_iflag & IXON){
		_raw_tty.c_iflag &= ~IXON;	/* turn off ^S/^Q on input   */
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }
	}
    }
}


/*
 *  Set up the tty driver to do LF->CR translation
 *
 * Args: state -- True to turn on translation, false to write raw LF's
 *
 * Result: none.
 */
void
crlf_proc(state)
int state;
{
    if(_inraw){
	if(state){				/* turn ON NL->CR on output */
	    if(!(_raw_tty.c_oflag & ONLCR)){
		_raw_tty.c_oflag |= ONLCR;
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }
	}
	else{					/* turn OFF NL-CR on output */
	    if(_raw_tty.c_oflag & ONLCR){
		_raw_tty.c_oflag &= ~ONLCR;
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }
	}
    }
}


/*
 *  Set up the tty driver to hanle interrupt char
 *
 * Args: state -- True to turn on interrupt char, false to not
 *
 * Result: tty driver that'll send us SIGINT or not
 */
void
intr_proc(state)
int state;
{
    if(_inraw){
	if(state){
	    _raw_tty.c_lflag |= ISIG;		/* enable signals */
	    (void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	}
	else{
	    _raw_tty.c_lflag &= ~ISIG;		/* disable signals */
	    (void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	}
    }
}


/*
 * Discard any pending input characters
 *
 * Args:  none
 *
 * Result: pending input buffer flushed
 */
void
flush_input()
{
    ioctl(STDIN_FD, TCFLSH, 0);
}


/*
 * Turn off hi bit stripping
 */
void
bit_strip_off()
{
}


/*
 * Turn off quit character (^\) if possible
 */
void
quit_char_off()
{
}


/*
 * Returns TRUE if tty is < 4800, 0 otherwise.
 */
int
ttisslow()
{
    return((_raw_tty.c_cflag&CBAUD) < (unsigned int)B4800);
}


