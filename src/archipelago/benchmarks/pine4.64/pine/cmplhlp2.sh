#!/bin/sh
#
# $Id: cmplhlp2.sh 7802 1998-02-19 01:45:56Z mikes $
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
# a .h file defining lots of strings
#


awk  ' BEGIN	{  printf("\n\n\t\t/*\n");
                   printf("\t\t * AUTMATICALLY GENERATED FILE!\n");
                   printf("\t\t * DO NOT EDIT!!\n\t\t */\n\n\n");
                   printf("#define\tHelpType\tchar **\n");
                   printf("#define\tNO_HELP\t(char **) NULL\n");
                   printf("struct _help_texts {\n");
                   printf("  HelpType help_text;\n");
                   printf("  char  *tag;\n};\n\n"); }
       /^====/  {  printf ("extern char *%s[] ; \n\n", $2 ); }
       END      {  printf ("\n\nextern struct _help_texts h_texts[];\n\n");}'




