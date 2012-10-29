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

static char rcsid[] = "$Id: bench.cpp,v 1.1 2003/10/17 02:39:03 emery Exp $";

#include <time.h>

#ifdef _WIN32          /* Windows 95/NT */
#ifdef __GNUG__        /* Cygnus Gnu C++ for win32*/
#include <windows.h>
#define clock() GetTickCount() /* quick fix for bug in clock() */
#endif
#endif

#include "robot.h"

#define NTRY 2000

Real PUMA560_data[] =
      {0.0, 0.0, 0.0, 0.0, M_PI/2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.35, 0.0, 0.0,
      0.0, 0.0, 0.0, 0.4318, 0.0, 17.4, -0.3638, 0.006, 0.2275, 0.13, 0.0, 0.0, 0.524, 0.0, 0.539,
      0.0, 0.0, 0.15005, 0.0203, -M_PI/2.0, 4.8, -0.0203, -0.0141, 0.07, 0.066, 0.0, 0.0, 0.086, 0.0, 0.0125,
      0.0, 0.0, 0.4318, 0.0, M_PI/2.0, 0.82, 0.0, 0.019, 0.0, 0.0018, 0.0, 0.0, 0.0013, 0.0, 0.0018,
      0.0, 0.0, 0.0, 0.0, -M_PI/2.0, 0.34, 0.0, 0.0, 0.0, 0.0003, 0.0, 0.0, 0.0004, 0.0, 0.0003,
      0.0, 0.0, 0.0, 0.0, 0.0, 0.09, 0.0, 0.0, 0.032, 0.00015, 0.0, 0.0, 0.00015, 0.0, 0.00004};

int main(void)
{
   int i;
   clock_t start, end;
   Matrix initrob(6,15), thomo, temp2;
   ColumnVector q(6), qs(6), temp;
   Robot robot;

   initrob << PUMA560_data;
   robot = Robot(initrob);
   q = M_PI/6.0;

   q = M_PI/4.0;
   printf("Begin compute Forward Kinematics\n");
   start = clock();
   for (i = 1; i <= NTRY; i++) {
      robot.set_q(q);
      temp2 = robot.kine();
   }
   end = clock();
   printf("MilliSeconds %6.2f\n", ((end - start) / (Real)CLOCKS_PER_SEC)*1000.0/NTRY);
   printf("end \n");

   qs = 1.0;
   qs(1) = M_PI/16.0;
   robot.set_q(q);
   thomo = robot.kine();
   printf("Begin compute Inverse Kinematics\n");
   start = clock();
   for (i = 1; i <= NTRY; i++) {
      robot.set_q(qs);
      temp = robot.inv_kin(thomo);
   }
   end = clock();
   printf("MilliSeconds %6.2f\n", ((end - start) / (Real)CLOCKS_PER_SEC)*1000.0/NTRY);
   printf("end \n");

   printf("Begin compute Jacobian\n");
   start = clock();
   for (i = 1; i <= NTRY; i++) {
      robot.set_q(q);
      temp2 = robot.jacobian();
   }
   end = clock();
   printf("MilliSeconds %6.2f\n", ((end - start) / (Real)CLOCKS_PER_SEC)*1000.0/NTRY);
   printf("end \n");

   printf("Begin compute Torque\n");
   start = clock();
   for (i = 1; i <= NTRY; i++) {
      temp = robot.torque(q,q,q);
   }
   end = clock();
   printf("MilliSeconds %6.2f\n", ((end - start) / (Real)CLOCKS_PER_SEC)*1000.0/NTRY);
   printf("end \n");

   printf("Begin acceleration\n");
   start = clock();
   for (i = 1; i <= NTRY; i++) {
      temp = robot.acceleration(q,q,q);
   }
   end = clock();
   printf("MilliSeconds %6.2f\n", ((end - start) / (Real)CLOCKS_PER_SEC)*1000.0/NTRY);
   printf("end \n");

   return 0;
}



