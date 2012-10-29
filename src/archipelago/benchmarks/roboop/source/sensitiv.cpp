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

static char rcsid[] = "$Id: sensitiv.cpp,v 1.1 2003/10/17 02:39:04 emery Exp $";

#include "robot.h"


ReturnMatrix Robot::dtau_dq(const ColumnVector & q, const ColumnVector & qp,
                           const ColumnVector & qpp)
{
   int i, j;
   Matrix ldtau_dq(dof,dof);
   ColumnVector ltorque(dof);
   ColumnVector dtorque(dof);
   ColumnVector dq(dof);
   for(i = 1; i <= dof; i++) {
      for(j = 1; j <= dof; j++) {
         dq(j) = (i == j ? 1.0 : 0.0);
      }
      dq_torque(q,qp,qpp,dq,ltorque,dtorque);
      for(j = 1; j <= dof; j++) {
         ldtau_dq(j,i) = dtorque(j);
      }
   }
   ldtau_dq.Release(); return ldtau_dq;
}

ReturnMatrix Robot::dtau_dqp(const ColumnVector & q, const ColumnVector & qp)
{
   int i, j;
   Matrix ldtau_dqp(dof,dof);
   ColumnVector ltorque(dof);
   ColumnVector dtorque(dof);
   ColumnVector dqp(dof);
   for(i = 1; i <= dof; i++) {
      for(j = 1; j <= dof; j++) {
         dqp(j) = (i == j ? 1.0 : 0.0);
      }
      dqp_torque(q,qp,dqp,ltorque,dtorque);
      for(j = 1; j <= dof; j++) {
         ldtau_dqp(j,i) = dtorque(j);
      }
   }
   ldtau_dqp.Release(); return ldtau_dqp;
}

