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

static char rcsid[] = "$Id: dynamics.cpp,v 1.1 2003/10/17 02:39:04 emery Exp $";

#include "robot.h"


ReturnMatrix Robot::torque(const ColumnVector & q, const ColumnVector & qp,
                           const ColumnVector & qpp)
{
   int i;
   ColumnVector z0(3);
   ColumnVector ltorque(dof);
   Matrix Rt, temp;
   ColumnVector *w, *wp, *vp, *a, *f, *n, *F, *N, *p;
   if(q.Ncols() != 1 || q.Nrows() != dof) error("q has wrong dimension");
   if(qp.Ncols() != 1 || qp.Nrows() != dof) error("qp has wrong dimension");
   if(qpp.Ncols() != 1 || qpp.Nrows() != dof) error("qpp has wrong dimension");
   set_q(q);
   w = new ColumnVector[dof+1];
   wp = new ColumnVector[dof+1];
   vp = new ColumnVector[dof+1];
   a = new ColumnVector[dof+1];
   f = new ColumnVector[dof+1];
   n = new ColumnVector[dof+1];
   F = new ColumnVector[dof+1];
   N = new ColumnVector[dof+1];
   p = new ColumnVector[dof+1];
   w[0] = ColumnVector(3);
   wp[0] = ColumnVector(3);
   vp[0] = gravity;
   z0(1) = z0(2) = 0.0;
   z0(3) = 1.0;
   w[0] = 0.0;
   wp[0] = 0.0;
   for(i = 1; i <= dof; i++) {
      Rt = links[i].R.t();
      p[i] = ColumnVector(3);
      p[i](1) = links[i].get_a();
      p[i](2) = links[i].get_d() * Rt(2,3);
      p[i](3) = links[i].get_d() * Rt(3,3);
      if(links[i].get_joint_type() == 0) {
         w[i] = Rt*(w[i-1] + z0*qp(i));
         wp[i] = Rt*(wp[i-1] + z0*qpp(i)
               + vec_x_prod(w[i-1],z0*qp(i)));
         vp[i] = vec_x_prod(wp[i],p[i])
               + vec_x_prod(w[i],vec_x_prod(w[i],p[i]))
               + Rt*(vp[i-1]);
      } else {
         w[i] = Rt*w[i-1];
         wp[i] = Rt*wp[i-1];
         vp[i] = Rt*(vp[i-1] + z0*qpp(i)
               + vec_x_prod(w[i],z0*qp(i)))* 2.0
               + vec_x_prod(wp[i],p[i])
               + vec_x_prod(w[i],vec_x_prod(w[i],p[i]));
      }
      a[i] = vec_x_prod(wp[i],links[i].r)
            + vec_x_prod(w[i],vec_x_prod(w[i],links[i].r))
            + vp[i];
   }

   for(i = dof; i >= 1; i--) {
      F[i] =  a[i] * links[i].m;
      N[i] = links[i].I*wp[i] + vec_x_prod(w[i],links[i].I*w[i]);
      if(i == dof) {
         f[i] = F[i];
         n[i] = vec_x_prod(p[i],f[i])
               + vec_x_prod(links[i].r,F[i]) + N[i];
      } else {
         f[i] = links[i+1].R*f[i+1] + F[i];
         n[i] = links[i+1].R*n[i+1] + vec_x_prod(p[i],f[i])
               + vec_x_prod(links[i].r,F[i]) + N[i];
      }
      if(links[i].get_joint_type() == 0) {
         temp = ((z0.t()*links[i].R)*n[i]);
         ltorque(i) = temp(1,1);
      } else {
         temp = ((z0.t()*links[i].R)*f[i]);
         ltorque(i) = temp(1,1);
      }
   }

   delete []p;
   delete []N;
   delete []F;
   delete []n;
   delete []f;
   delete []a;
   delete []vp;
   delete []wp;
   delete []w;
   ltorque.Release(); return ltorque;
}

ReturnMatrix Robot::torque_novelocity(const ColumnVector & q,
               const ColumnVector & qpp)
{
   int i;
   ColumnVector z0(3);
   ColumnVector ltorque(dof);
   Matrix Rt, temp;
   ColumnVector *wp, *vp, *a, *f, *n, *F, *N, *p;
   if(q.Ncols() != 1 || q.Nrows() != dof) error("q has wrong dimension");
   if(qpp.Ncols() != 1 || qpp.Nrows() != dof) error("qpp has wrong dimension");
   set_q(q);
   wp = new ColumnVector[dof+1];
   vp = new ColumnVector[dof+1];
   a = new ColumnVector[dof+1];
   f = new ColumnVector[dof+1];
   n = new ColumnVector[dof+1];
   F = new ColumnVector[dof+1];
   N = new ColumnVector[dof+1];
   p = new ColumnVector[dof+1];
   wp[0] = ColumnVector(3);
   vp[0] = ColumnVector(3);
   z0(1) = z0(2) = 0.0;
   z0(3) = 1.0;
   vp[0] = 0.0;
   wp[0] = 0.0;
   for(i = 1; i <= dof; i++) {
      Rt = links[i].R.t();
      p[i] = ColumnVector(3);
      p[i](1) = links[i].get_a();
      p[i](2) = links[i].get_d() * Rt(2,3);
      p[i](3) = links[i].get_d() * Rt(3,3);
      if(links[i].get_joint_type() == 0) {
         wp[i] = Rt*(wp[i-1] + z0*qpp(i));
         vp[i] = vec_x_prod(wp[i],p[i])
               + Rt*(vp[i-1]);
      } else {
         wp[i] = Rt*wp[i-1];
         vp[i] = Rt*(vp[i-1] + z0*qpp(i))
               + vec_x_prod(wp[i],p[i]);
      }
      a[i] = vec_x_prod(wp[i],links[i].r) + vp[i];
   }

   for(i = dof; i >= 1; i--) {
      F[i] = a[i] * links[i].m;
      N[i] = links[i].I*wp[i];
      if(i == dof) {
         f[i] = F[i];
         n[i] = vec_x_prod(p[i],f[i])
               + vec_x_prod(links[i].r,F[i]) + N[i];
      } else {
         f[i] = links[i+1].R*f[i+1] + F[i];
         n[i] = links[i+1].R*n[i+1] + vec_x_prod(p[i],f[i])
               + vec_x_prod(links[i].r,F[i]) + N[i];
      }
      if(links[i].get_joint_type() == 0) {
         temp = ((z0.t()*links[i].R)*n[i]);
         ltorque(i) = temp(1,1);
      } else {
         temp = ((z0.t()*links[i].R)*f[i]);
         ltorque(i) = temp(1,1);
      }
   }

   delete []p;
   delete []N;
   delete []F;
   delete []n;
   delete []f;
   delete []a;
   delete []vp;
   delete []wp;
   ltorque.Release(); return ltorque;
}

ReturnMatrix Robot::inertia(const ColumnVector & q)
{
   int i, j;
   Matrix M(dof,dof);
   ColumnVector torque(dof);
   for(i = 1; i <= dof; i++) {
      for(j = 1; j <= dof; j++) {
         torque(j) = (i == j ? 1.0 : 0.0);
      }
      torque = torque_novelocity(q,torque);
      M.Column(i) = torque;
/*    for(j = 1; j <= dof; j++) {
         M(j,i) = torque(j);
      }*/
   }
   M.Release(); return M;
}

ReturnMatrix Robot::acceleration(const ColumnVector & q,
      const ColumnVector & qp, const ColumnVector & tau)
{
   ColumnVector qpp(dof);

   qpp = 0.0;
   qpp = inertia(q).i()*(tau-torque(q,qp,qpp));
   qpp.Release(); return qpp;
}
