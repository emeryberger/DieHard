#!/bin/sh
#
# $Id: cmplhelp.sh 7841 1998-02-21 00:49:31Z mikes $
#
#            T H E    P I N E    M A I L   S Y S T E M
#
#   Laurence Lundblade and Mike Seibel
#   Networks and Distributed Computing
#   Computing and Communications
#   University of Washington
#   Administration Building, AG-44
#   Seattle, Washington, 98195, USA
#   Internet: lgl@CAC.Washington.EDU
#             mikes@CAC.Washington.EDU
#
#   Please address all bugs and comments to "pine-bugs@cac.washington.edu"
#
#
#   Pine and Pico are registered trademarks of the University of Washington.
#   No commercial use of these trademarks may be made without prior written
#   permission of the University of Washington.
#
#   Pine, Pico, and Pilot software and its included text are Copyright
#   1989-1996 by the University of Washington.
#
#   The full text of our legal notices is contained in the file called
#   CPYRIGHT, included with this distribution.
#
#
#   Pine is in part based on The Elm Mail System:
#    ***********************************************************************
#    *  The Elm Mail System  -  Revision: 2.13                             *
#    *                                                                     *
#    * 			Copyright (c) 1986, 1987 Dave Taylor               *
#    * 			Copyright (c) 1988, 1989 USENET Community Trust    *
#    ***********************************************************************
# 
#


# 
# cmplhelp.sh -- This script take the pine.help file and turns it into
# a .c file defining lots of strings
#

sed '
	s/\\"/-sed-backslash-quote-/g
	s/"/\\"/g
	s/-sed-backslash-quote-/\\"/g
    ' |

awk 'BEGIN         {in_text = 0;
                    count = 0;
                    printf("#include <stdio.h>\n#include \"headers.h\"\n\n\n");
                   }


     /^====/       { if(in_text)
                         printf("NULL\n};\n\n\n");
                     printf ("char *%s[] = {\n ", $2 );
                     texts[count++] = $2;
                     in_text = 1;
                    }
     /^$/           { if(in_text){
                          printf("\" \",\n");
                      }
                    }
     /^[^====]/     { if(in_text)
                          printf("\"%s\",\n", $0);
                    }
     END            { if(in_text)
                          printf("NULL\n};\n\n");
                      printf("struct _help_texts h_texts[] = {\n");
                      for(i = 0; i < count; i++)
                          printf("{%s,\"%s\"},\n",texts[i], texts[i]);
                      printf("{NO_HELP, NULL}\n};\n\n");
                    } '
