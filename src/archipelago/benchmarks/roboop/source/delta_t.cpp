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

static char rcsid[] = "$Id: delta_t.cpp,v 1.1 2003/10/17 02:39:03 emery Exp $";

#include "robot.h"


void Robot::delta_torque(const ColumnVector & q, const ColumnVector & qp,
                        const ColumnVector & qpp, const ColumnVector & dq,
                        const ColumnVector & dqp, const ColumnVector & dqpp,
                        ColumnVector & ltorque, ColumnVector & dtorque)
{
   int i;
   ColumnVector z0(3);
   Matrix Rt, temp;
   Matrix Q(3,3);
   ColumnVector *w, *wp, *vp, *a, *f, *n, *F, *N, *p;
   ColumnVector *dw, *dwp, *dvp, *da, *df, *dn, *dF, *dN, *dp;
   if(q.Ncols() != 1 || q.Nrows() != dof) error("q has wrong dimension");
   if(qp.Ncols() != 1 || qp.Nrows() != dof) error("qp has wrong dimension");
   if(qpp.Ncols() != 1 || qpp.Nrows() != dof) error("qpp has wrong dimension");
   ltorque = ColumnVector(dof);
   dtorque = ColumnVector(dof);
   set_q(q);
   z0 = 0.0;
   Q = 0.0;
   w = new ColumnVector[dof+1];
   wp = new ColumnVector[dof+1];
   vp = new ColumnVector[dof+1];
   a = new ColumnVector[dof+1];
   f = new ColumnVector[dof+1];
   n = new ColumnVector[dof+1];
   F = new ColumnVector[dof+1];
   N = new ColumnVector[dof+1];
   p = new ColumnVector[dof+1];
   dw = new ColumnVector[dof+1];
   dwp = new ColumnVector[dof+1];
   dvp = new ColumnVector[dof+1];
   da = new ColumnVector[dof+1];
   df = new ColumnVector[dof+1];
   dn = new ColumnVector[dof+1];
   dF = new ColumnVector[dof+1];
   dN = new ColumnVector[dof+1];
   dp = new ColumnVector[dof+1];
   w[0] = ColumnVector(3);
   wp[0] = ColumnVector(3);
   vp[0] = gravity;
   dw[0] = ColumnVector(3);
   dwp[0] = ColumnVector(3);
   dvp[0] = ColumnVector(3);
   z0 = 0.0;
   Q = 0.0;
   Q(1,2) = -1.0;
   Q(2,1) = 1.0;
   z0(3) = 1.0;
   w[0] = 0.0;
   wp[0] = 0.0;
   dw[0] = 0.0;
   dwp[0] = 0.0;
   dvp[0] = 0.0;
   for(i = 1; i <= dof; i++) {
      Rt = links[i].R.t();
      p[i] = ColumnVector(3);
      p[i](1) = links[i].get_a();
      p[i](2) = links[i].get_d() * Rt(2,3);
      p[i](3) = links[i].get_d() * Rt(3,3);
      if(links[i].get_joint_type() != 0) {
         dp[i] = ColumnVector(3);
         dp[i](1) = 0.0;
         dp[i](2) = Rt(2,3);
         dp[i](3) = Rt(3,3);
      }
      if(links[i].get_joint_type() == 0) {
         w[i] = Rt*(w[i-1] + z0*qp(i));
         dw[i] = Rt*(dw[i-1] + z0*dqp(i)
               - Q*w[i-1]*dq(i));
         wp[i] = Rt*(wp[i-1] + z0*qpp(i)
               + vec_x_prod(w[i-1],z0*qp(i)));
         dwp[i] = Rt*(dwp[i-1] + z0*dqpp(i)
               + vec_x_prod(dw[i-1],z0*qp(i))
               + vec_x_prod(w[i-1],z0*dqp(i))
               - Q*(wp[i-1]+vec_x_prod(w[i-1],z0*qp(i)))
                  *dq(i));
         vp[i] = vec_x_prod(wp[i],p[i])
               + vec_x_prod(w[i],vec_x_prod(w[i],p[i]))
               + Rt*(vp[i-1]);
         dvp[i] = vec_x_prod(dwp[i],p[i])
               + vec_x_prod(dw[i],vec_x_prod(w[i],p[i]))
               + vec_x_prod(w[i],vec_x_prod(dw[i],p[i]))
               + Rt*(dvp[i-1] - Q*vp[i-1]*dq(i));
      } else {
         w[i] = Rt*w[i-1];
         dw[i] = Rt*dw[i-1];
         wp[i] = Rt*wp[i-1];
         dwp[i] = Rt*dwp[i-1];
         vp[i] = Rt*(vp[i-1] + z0*qpp(i)
               + vec_x_prod(w[i],z0*qp(i))) * 2.0
               + vec_x_prod(wp[i],p[i])
               + vec_x_prod(w[i],vec_x_prod(w[i],p[i]));
         dvp[i] = Rt*(dvp[i-1] + z0*dqpp(i)
               + (vec_x_prod(dw[i],z0*qp(i)) * 2.0
               + vec_x_prod(w[i],z0*dqp(i))))
               + vec_x_prod(dwp[i],p[i])
               + vec_x_prod(dw[i],vec_x_prod(w[i],p[i]))
               + vec_x_prod(w[i],vec_x_prod(dw[i],p[i]))
               + (vec_x_prod(wp[i],dp[i])
               + vec_x_prod(w[i],vec_x_prod(w[i],dp[i])))
                  *dq(i);
      }
      a[i] = vec_x_prod(wp[i],links[i].r)
            + vec_x_prod(w[i],vec_x_prod(w[i],links[i].r))
            + vp[i];
      da[i] = vec_x_prod(dwp[i],links[i].r)
            + vec_x_prod(dw[i],vec_x_prod(w[i],links[i].r))
            + vec_x_prod(w[i],vec_x_prod(dw[i],links[i].r))
            + dvp[i];
   }

   for(i = dof; i >= 1; i--) {
      F[i] = a[i] * links[i].m;
      N[i] = links[i].I*wp[i] + vec_x_prod(w[i],links[i].I*w[i]);
      dF[i] = da[i] * links[i].m;
      dN[i] = links[i].I*dwp[i] + vec_x_prod(dw[i],links[i].I*w[i])
            + vec_x_prod(w[i],links[i].I*dw[i]);
      if(i == dof) {
         f[i] = F[i];
         n[i] = vec_x_prod(p[i],f[i])
               + vec_x_prod(links[i].r,F[i]) + N[i];
         df[i] = dF[i];
         dn[i] = vec_x_prod(p[i],df[i])
               + vec_x_prod(links[i].r,dF[i]) + dN[i];
         if(links[i].get_joint_type() != 0) {
            dn[i] = dn[i] + vec_x_prod(dp[i],f[i])*dq(i);
         }
      } else { 
         f[i] = links[i+1].R*f[i+1] + F[i];
         n[i] = links[i+1].R*n[i+1] + vec_x_prod(p[i],f[i])
               + vec_x_prod(links[i].r,F[i]) + N[i];
         if(links[i+1].get_joint_type() == 0) {
            df[i] = links[i+1].R*df[i+1] + dF[i] 
                  + Q*links[i+1].R*f[i+1]*dq(i+1);
         } else {
            df[i] = links[i+1].R*df[i+1] + dF[i];
         }
         dn[i] = links[i+1].R*dn[i+1] + vec_x_prod(p[i],df[i])
               + vec_x_prod(links[i].r,dF[i]) + dN[i];
         if(links[i+1].get_joint_type() == 0) {
            dn[i] = dn[i] + Q*links[i+1].R*n[i+1]*dq(i+1);
         }
         if(links[i].get_joint_type() != 0) {
            dn[i] = dn[i] + vec_x_prod(dp[i],f[i])*dq(i);
         }
      }
      if(links[i].get_joint_type() == 0) {
         temp = ((z0.t()*links[i].R)*n[i]);
         ltorque(i) = temp(1,1);
         temp = ((z0.t()*links[i].R)*dn[i]);
         dtorque(i) = temp(1,1);
      } else {
         temp = ((z0.t()*links[i].R)*f[i]);
         ltorque(i) = temp(1,1);
         temp = ((z0.t()*links[i].R)*df[i]);
         dtorque(i) = temp(1,1);
      }
   }

   delete []dp;
   delete []dN;
   delete []dF;
   delete []dn;
   delete []df;
   delete []da;
   delete []dvp;
   delete []dwp;
   delete []dw;
   delete []p;
   delete []N;
   delete []F;
   delete []n;
   delete []f;
   delete []a;
   delete []vp;
   delete []wp;
   delete []w;
}

