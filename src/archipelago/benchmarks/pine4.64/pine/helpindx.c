#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: helpindx.c 9602 1999-01-06 16:31:16Z mikes $";
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
   1989-1999 by the University of Washington.

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

/*
 * very short, very specialized
 *
 *
 *
 */
#include <stdio.h>
#include <ctype.h>

#define	HELP_KEY_MAX	64		/* maximum length of a key */

struct hindx {
    char  key[HELP_KEY_MAX];		/* name of help section */
    long  offset;			/* where help text starts */
    short lines;			/* how many lines there are */
};

main(argc, argv)
int  argc;
char **argv;
{
    char *p, s[1024];
    long index;
    int  section, 
	 len, 
	 line,
	 i; 
    FILE *hp,
	 *hip,					/* help index ptr */
    	 *hhp;					/* help header ptr */
    struct hindx irec;

    if(argc < 4){
	fprintf(stderr,
		"usage: helpindx <help_file> <index_file> <header_file>\n");
	exit(-1);
    }

    if((hp = fopen(argv[1], "rb")) == NULL){	/* problems */
	perror(argv[1]);
        exit(-1);
    }

    if((hip = fopen(argv[2], "wb")) == NULL){	/* problems */
	perror(argv[2]);
        exit(-1);
    }

    if((hhp = fopen(argv[3], "w")) == NULL){	/* problems */
	perror(argv[3]);
        exit(-1);
    }

    fprintf(hhp,"/*\n * Pine Help text header file\n */\n");
    fprintf(hhp,"\n#define\tHELP_KEY_MAX\t%d\n", HELP_KEY_MAX);
    fprintf(hhp,"\ntypedef\tshort\tHelpType;\n");
    fprintf(hhp,"\n#define\tNO_HELP\t(-1)\n");
    fprintf(hhp,"struct hindx {\n    char  key[HELP_KEY_MAX];");
    fprintf(hhp,"\t\t/* name of help section */\n");
    fprintf(hhp,"    long  offset;\t\t\t/* where help text starts */\n");
    fprintf(hhp,"    short lines;\t\t\t/* how many lines there are */\n");
    fprintf(hhp,"};\n\n\n/*\n * defs for help section titles\n */\n");

    index = 0L;
    line  = section = 0;

    while(fgets(s, sizeof(s) - 1, hp) != NULL){
	line++;
	len = strlen(s);
	if(s[0] == '='){			/* new section? */
	    i = 0;
	    while((s[i] == '=' || isspace((unsigned char)s[i])) && i < len)
		i++;

	    if(section)
	        fwrite(&irec, sizeof(struct hindx), 1, hip);

	    irec.offset = index + (long)i;	/* save where name starts */
	    irec.lines = 0;
	    p = &irec.key[0];			/* save name field */
	    while(!isspace((unsigned char)s[i]) && i < len)
		*p++ = s[i++];
	    *p = '\0';
	
	    if(irec.key[0] == '\0'){
		fprintf(stderr,"Invalid help line %d: %s", line, s);
		exit(-1);
	    }
	    else
	      fprintf(hhp, "#define\t%s\t%d\n", irec.key, section++);

	}
	else if(s[0] == '#' && section){
	    fprintf(stderr,"Comments not allowed in help text: line %d", line);
	    exit(-1);
	}
	else{
	    irec.lines++;
	}
	index += len;
    }

    if(section)				/* write last entry */
      fwrite(&irec, sizeof(struct hindx), 1, hip);

    fprintf(hhp, "#define\tLASTHELP\t%d\n", section);

    fclose(hp);
    fclose(hip);
    fclose(hhp);
}
