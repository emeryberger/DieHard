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

static char rcsid[] = "$Id: kinemat.cpp,v 1.1 2003/10/17 02:39:04 emery Exp $";

#include "robot.h"

#define NITMAX 100  /* maximum number of iterations in inv_kin */
#define ITOL   1e-6 /* tolerance for the end of iterations in inv_kin */

void Robot::kine(Matrix & Rot, ColumnVector & pos)
{
   kine(Rot,pos,dof);
}

void Robot::kine(Matrix & Rot, ColumnVector & pos, int j)
{
   int i;

   if(j < 1 || j > dof) error("j must be 1 <= j <= dof");

   Rot = links[1].R;
   pos = links[1].p;
   for (i = 2; i <= j; i++) {
      pos = pos + Rot*links[i].p;
      Rot = Rot*links[i].R;
   }
}

ReturnMatrix Robot::kine(void)
{
   Matrix thomo;

   thomo = kine(dof);
   thomo.Release(); return thomo;
}

ReturnMatrix Robot::kine(int j)
{
   Matrix Rot, thomo(4,4);
   ColumnVector pos;

   kine(Rot,pos,j);
   thomo << fourbyfourident;
   thomo.SubMatrix(1,3,1,3) = Rot;
   thomo.SubMatrix(1,3,4,4) = pos;
   thomo.Release(); return thomo;
}

void Robot::dTdqi(Matrix & dRot, ColumnVector & dp, int i)
{
   int j;

   if(i < 1 || i > dof) error("i must be 1 <= i <= dof");
   if(links[i].get_joint_type() == 0) {
      Matrix dR(3,3);
      dR = 0.0;
      Matrix R2 = links[i].R;
      Matrix p2 = links[i].p;
      dRot = Matrix(3,3);
      dRot << threebythreeident;
      for(j = 1; j < i; j++) {
         dRot = dRot*links[j].R;
      }
      // dRot * Q
      for (j = 1; j <= 3; j++) {
         dR(j,1) = dRot(j,2);
         dR(j,2) = -dRot(j,1);
      }
      for(j = i+1; j <= dof; j++) {
         p2 = p2 + R2*links[j].p;
         R2 = R2*links[j].R;
      }
      dp = dR*p2;
      dRot = dR*R2;
   } else {
      dRot = Matrix(3,3);
      dp = Matrix(3,1);
      dRot = 0.0;
      dp = 0.0;
      dp(3) = 1.0;
      for(j = i-1; j >= 1; j--) {
         dp = links[j].R*dp;
      }
   }
}

ReturnMatrix Robot::dTdqi(int i)
{
   Matrix dRot, thomo(4,4);
   ColumnVector dpos;

   dTdqi(dRot, dpos, i);
   thomo = (Real) 0.0;
   thomo.SubMatrix(1,3,1,3) = dRot;
   thomo.SubMatrix(1,3,4,4) = dpos;
   thomo.Release(); return thomo;
}

ReturnMatrix Robot::jacobian(int ref)
{
   int i;
   Matrix jac(6,dof);
   Matrix *R, *p;
   Matrix pr, temp(3,1);

   if(ref < 0 || ref > dof) error("invalid referential");
   R = new Matrix[dof+1];
   p = new Matrix[dof+1];

   R[0] = Matrix(3,3);
   R[0] << threebythreeident;
   p[0] = Matrix(3,1);
   p[0] = (Real) 0.0;
   for(i = 1; i <= dof; i++) {
      R[i] = R[i-1]*links[i].R;
      p[i] = p[i-1]+R[i-1]*links[i].p;
   }

   for(i = 1; i <= dof; i++) {
      if(links[i].get_joint_type() == 0) {
         temp(1,1) = R[i-1](1,3);
         temp(2,1) = R[i-1](2,3);
         temp(3,1) = R[i-1](3,3);
         pr = p[dof]-p[i-1];
         temp = vec_x_prod(temp,pr);
         jac(1,i) = temp(1,1);
         jac(2,i) = temp(2,1);
         jac(3,i) = temp(3,1);
         jac(4,i) = R[i-1](1,3);
         jac(5,i) = R[i-1](2,3);
         jac(6,i) = R[i-1](3,3);
      } else {
         jac(1,i) = R[i-1](1,3);
         jac(2,i) = R[i-1](2,3);
         jac(3,i) = R[i-1](3,3);
         jac(4,i) = jac(5,i) = jac(6,i) = 0.0;
      }
   }
   if(ref != 0) {
      Matrix zeros(3,3);
      zeros = (Real) 0.0;
      Matrix RT = R[ref].t();
      Matrix Rot;
      Rot = ((RT & zeros) | (zeros & RT));
      jac = Rot*jac;
   }

   delete []R;
   delete []p;
   jac.Release(); return jac;
}

ReturnMatrix Robot::inv_kin(const Matrix & Tobj, int mj)
{
   int i, j;
   ColumnVector qout, dq;
   Matrix B, M;
   UpperTriangularMatrix U;

   qout = get_q();

   if(mj ==0) { /* Jacobian based */
      Matrix Ipd, A, B(6,1);
      for(j = 1; j <= NITMAX; j++) {
         Ipd = (kine()).i()*Tobj;
         B(1,1) = Ipd(1,4);
         B(2,1) = Ipd(2,4);
         B(3,1) = Ipd(3,4);
         B(4,1) = Ipd(3,2);
         B(5,1) = Ipd(1,3);
         B(6,1) = Ipd(2,1);
         A = jacobian(dof);
         QRZ(A,U);
         QRZ(A,B,M);
         dq = U.i()*M;
         qout = qout + dq;
         set_q(qout);
         if (dq.MaximumAbsoluteValue() < ITOL) break;
      }
   } else { /* using partial derivative of T  */
      Matrix A(12,dof);
      for(j = 1; j <= NITMAX; j++) {
         B = (Tobj-kine()).SubMatrix(1,3,1,4).AsColumn();
         for(i = 1; i <= dof; i++) {
            A.SubMatrix(1,12,i,i) = dTdqi(i).SubMatrix(1,3,1,4).AsColumn();
         }
         QRZ(A,U);
         QRZ(A,B,M);
         dq = U.i()*M;
         qout = qout + dq;
         set_q(qout);
         if (dq.MaximumAbsoluteValue() < ITOL) break;
      }
   }

   qout.Release(); return qout;
}


