/*
ROBOOP -- A robotics object oriented package in C++
Copyright (C) 1996, 1997  Richard Gourdeau

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library (GNUlgpl.txt); if not, write to the
Free Software Foundation, Inc., 675 Mass Ave, Cambridge,
MA 02139, USA.

Report problems and direct all questions to:

Richard Gourdeau
Professeur Agrégé
Departement de mathematiques et de genie industriel
Ecole Polytechnique de Montreal
C.P. 6079, Succ. Centre-Ville
Montreal, Quebec, H3C 3A7

email: richard.gourdeau@polymtl.ca
*/

static char rcsid[] = "$Id: gnugraph.cpp,v 1.1 2003/10/17 02:39:04 emery Exp $";

#if defined(__WIN32__) || defined(_WIN32) || defined(__NT__)       /* Windows 95/NT */

#define GNUPLOT "wgnupl32.exe"
#define STRICT
#include <windows.h>

#else /* Unix */

#define GNUPLOT "gnuplot"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include "robot.h"
#include <string.h>
#include <fstream>

using namespace std;


char *curvetype[] =
   {"lines",
    "points",
    "linespoints",
    "impulses",
    "dots",
    "steps",
    "boxes"};

GNUcurve::GNUcurve(void)
{
   ctype = LINES;
   clabel = new char[1];
   strcpy(clabel,"");
}

GNUcurve::GNUcurve(const Matrix & data, const char * label, int type)
{
   xy = data;
   if(data.Ncols() > 2) {
      cout << "Warning: Only the first two columns are used in GNUcurve\n";
   }
   ctype = type;
   clabel = new char[strlen(label)+1];
   strcpy(clabel,label);
}

GNUcurve::GNUcurve(const GNUcurve & gnuc)
{
   xy = gnuc.xy;
   ctype = gnuc.ctype;
   clabel = new char[strlen(gnuc.clabel)+1];
   strcpy(clabel,gnuc.clabel);
}

GNUcurve & GNUcurve::operator=(const GNUcurve & gnuc)
{
   xy = gnuc.xy;
   ctype = gnuc.ctype;
   delete clabel;
   clabel = new char[strlen(gnuc.clabel)+1];
   strcpy(clabel,gnuc.clabel);
   return *this;
}

GNUcurve::~GNUcurve(void)
{
   delete [] clabel;
}

void GNUcurve::dump(void)
{
   cout << "Curve label: " << clabel << "\n";
   cout << "Curve type:  " << curvetype[ctype] << "\n";
   cout << "Curve data points: \n";
   cout << xy;
   cout << "\n\n";
}

void Plot2d::gnuplot(void)
{
   int i, strl;
   char filename[L_tmpnam];
   char * filedata;
   char * wibsl;
   char bsl = '\\';

   tmpnam(filename); /* generate a temporary file name */
   /* replacing \ by / */
   while((wibsl = strchr(filename,bsl)) != NULL) {
      wibsl[0] = '/';
   }

   {
      ofstream fileout(filename); /* write the command file */
      if(gnucommand != NULL) {
         fileout << gnucommand;
      }
      if(title != NULL) {
         fileout << "set title \"" << title << "\"\n";
      }
      if(xlabel  != NULL) {
         fileout << "set xlabel \"" << xlabel << "\"\n";
      }
      if(ylabel != NULL) {
         fileout << "set ylabel \"" << ylabel << "\"\n";
      }
      fileout << "plot \\\n";
      for(i = 0; i < ncurves; i++) {
         fileout << "\"" << filename << "." << i << "\" ";
         fileout << "title \"" << curves[i].clabel << "\" ";
         fileout << "with " << curvetype[curves[i].ctype] << " ";
         if( i+1 < ncurves){
            fileout << ", \\\n";
         }
      }
      fileout << "\n";
   }
   filedata = new char[strlen(filename)+3];
   strcpy(filedata,filename);
   strcat(filedata,".");
   strl = strlen(filedata);
   for(i = 0; i < ncurves; i++) { /* write the data files */
      sprintf(&filedata[strl],"%d",i);
      ofstream fileout(filedata);
      fileout << curves[i].xy;
   }
   /* all command and data files are ready for the call to gnuplot */
#if defined(__WIN32__) || defined(_WIN32) || defined(__NT__) /* Windows 95/NT */
   char c[L_tmpnam+15];
   char *d;
   HWND hwndm, hwnd;
   if (WinExec(GNUPLOT, SW_SHOWNORMAL) <= 32) { /* start gnuplot */
      /* failed */
      cout << "Cannot find the gnuplot application\n";
      cout << "Press Enter to continue\n";
      getchar();
      remove(filename); /* clean up the files and return */
      for(i = 0; i < ncurves; i++) {
         sprintf(&filedata[strl],"%d",i);
         remove(filedata);
      }
      delete filedata;
      return;
   } else { /* succeed */
 		/* get gnuplot main window handle */
      hwndm = FindWindow((LPSTR) NULL, (LPSTR) "gnuplot");
   }
   hwnd= GetWindow(hwndm, GW_CHILD); /* get gnuplot command area handle */
   if(hwnd == NULL) cout << "OUPS!!!\n"; /* something wrong happened */
   sprintf(c,"load \"%s\" \n",filename); /* load command for the plot */

#ifdef __GNUG__        /* Cygnus Gnu C++ for win32*/
   char ccygnus[] = "cd \"c:\"\n"; /* this string should reflect
                                      the drive used to mount / 
                                      where /tmp is located */
   d = ccygnus;
   while(*d != '\0') { /* sending the command through windows messages */
      SendMessage(hwnd,WM_CHAR,*d,1L);
      d++;
   }
#endif
   d = c;
   while(*d != '\0') { /* sending the command through windows messages */
      SendMessage(hwnd,WM_CHAR,*d,1L);
      d++;
   }
   cout << "Press Enter to continue...\n"; 
   /* flush output, needed by Visual C++ */
   flush(cout);
   getchar();
#else      /*  using a pipe under Unix */
   FILE *command;

   command = popen(GNUPLOT,"w");
   fprintf(command,"load \"%s\" with lines\n",filename); fflush(command);
   fprintf(stderr,"Press Enter to continue...\n"); fflush(stderr);
   getchar();
   pclose(command);
#endif
   remove(filename);
   for(i = 0; i < ncurves; i++) {
      sprintf(&filedata[strl],"%d",i);
      remove(filedata);
   }
   delete filedata;
}

void Plot2d::addcurve(const Matrix & data, const char * label, int type)
{
   ncurves++;
   if(ncurves <= NCURVESMAX) {
      curves[ncurves-1] = GNUcurve(data,label,type);
   } else {
      cout << "Too many curves in your Plot2d (maximum 10)\n";
      exit(1);
   }
}

void Plot2d::addcommand(const char * gcom)
{
   char * tempcom;

   if(gnucommand !=  NULL) {
      tempcom = strdup(gnucommand);
      delete gnucommand;
   } else {
      tempcom = strdup("");
   }
   gnucommand = new char[strlen(tempcom)+strlen(gcom)+strlen("\n")+1];
   strcpy(gnucommand,tempcom);
   strcat(gnucommand,gcom);
   strcat(gnucommand,"\n");
   free(tempcom);
}

void Plot2d::settitle(const char * t)
{
   if(title !=  NULL) delete title;
   title = new char[strlen(t)+1];
   strcpy(title,t);

}

void Plot2d::setxlabel(const char * t)
{
   if(xlabel !=  NULL) delete xlabel;
   xlabel = new char[strlen(t)+1];
   strcpy(xlabel,t);

}

void Plot2d::setylabel(const char * t)
{
   if(ylabel !=  NULL) delete ylabel;
   ylabel = new char[strlen(t)+1];
   strcpy(ylabel,t);

}

void Plot2d::dump(void)
{
   int i;

   if(gnucommand != NULL) {
      cout << "gnuplot commands:\n" << gnucommand;
   }
   if(title != NULL) {
      cout << "Plot title: " << title << "\n";
   }
   if(xlabel != NULL) {
      cout << "X label:    " << xlabel << "\n";
   }
   if(ylabel != NULL) {
      cout << "Y label:    " << ylabel << "\n";
   }
   for (i = 0; i < ncurves; i++) {
      cout << "\nCurve #" << i << "\n";
      curves[i].dump();
   }
}

Plot2d::Plot2d(void)
{
   title = NULL;
   xlabel = NULL;
   ylabel = NULL;
   gnucommand = NULL;
   ncurves = 0;
   curves = new GNUcurve[NCURVESMAX];
}

Plot2d::~Plot2d(void)
{
   if(title != NULL) delete title;
   if(xlabel != NULL) delete xlabel;
   if(ylabel != NULL) delete ylabel;
   if(gnucommand != NULL) delete gnucommand;
   delete [] curves;
}



