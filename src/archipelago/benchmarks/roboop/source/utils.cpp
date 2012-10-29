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

static char rcsid[] = "$Id: utils.cpp,v 1.1 2003/10/17 02:39:04 emery Exp $";

#include "robot.h"

/* vector cross product */
ReturnMatrix vec_x_prod(const ColumnVector & x, const ColumnVector & y)
{
   ColumnVector z(3);

   z(1) = x(2)*y(3) - x(3)*y(2);
   z(2) = x(3)*y(1) - x(1)*y(3);
   z(3) = x(1)*y(2) - x(2)*y(1);

   z.Release(); return z;
}

/* fixed step size fourth-order Runge-Kutta integrator */
void Runge_Kutta4(ReturnMatrix (*xdot)(Real time, const Matrix & xin),
            const Matrix & xo, Real to, Real tf, int nsteps,
            RowVector & tout, Matrix & xout)
{
   int i;
   Real h, h2, t;
   Matrix k1, k2, k3, k4, x;

   h = (tf-to)/nsteps;
   h2 = h/2.0;
   t = to;
   x = xo;
   xout = Matrix(xo.Nrows(),(nsteps+1)*xo.Ncols());
   xout.SubMatrix(1,xo.Nrows(),1,xo.Ncols()) = x;
   tout = RowVector(nsteps+1);
   tout(1) = to;
   for (i = 1; i <= nsteps; i++) {
      k1 = (*xdot)(t, x) * h;
      k2 = (*xdot)(t+h2, x+k1*0.5)*h;
      k3 = (*xdot)(t+h2, x+k2*0.5)*h;
      k4 = (*xdot)(t+h, x+k3)*h;
      x = x + (k1*0.5+ k2 + k3 + k4*0.5)*(1.0/3.0);
      t += h;
      tout(i+1) = t;
      xout.SubMatrix(1,xo.Nrows(),i*xo.Ncols()+1,(i+1)*xo.Ncols()) = x;
   }
}

/* compute one Runge-Kutta fourth order step */
/* adapted from:
   Numerical Recipes in C, The Art of Scientific Computing,
   Press, William H. and Flannery, Brian P. and Teukolsky, Saul A.
   and Vetterling, William T., Cambridge University Press, 1988.
*/
ReturnMatrix rk4(const Matrix & x, const Matrix & dxdt, Real t, Real h,
                  ReturnMatrix (*xdot)(Real time, const Matrix & xin))
{
   Matrix xt, xout, dxm, dxt;
   Real th, hh, h6;

   hh = h*0.5;
   h6 = h/6.0;
   th = t + hh;
   xt = x + hh*dxdt;
   dxt = (*xdot)(th,xt);
   xt = x + hh*dxt;
   dxm = (*xdot)(th,xt);
   xt = x + h*dxm;
   dxm += dxt;
   dxt = (*xdot)(t+h,xt);
   xout = x+h6*(dxdt+dxt+2.0*dxm);

   xout.Release(); return xout;
}

#define PGROW -0.20
#define PSHRNK -0.25
#define FCOR 0.06666666
#define SAFETY 0.9
#define ERRCON 6.0E-4

/* compute one adaptive step based on two rk4 */
/* adapted from:
   Numerical Recipes in C, The Art of Scientific Computing,
   Press, William H. and Flannery, Brian P. and Teukolsky, Saul A.
   and Vetterling, William T., Cambridge University Press, 1988.
*/
void rkqc(Matrix & x, Matrix & dxdt, Real & t, Real htry,
          Real eps, Matrix & xscal, Real & hdid, Real & hnext,
          ReturnMatrix (*xdot)(Real time, const Matrix & xin))
{
   int i;
   Real tsav, hh, h, temp, errmax;
   Matrix dxsav, xsav, xtemp;

   tsav = t;
   xsav = x;
   dxsav = dxdt;
   h = htry;
   for(;;) {
      hh = 0.5*h;
      xtemp = rk4(xsav,dxsav,tsav,hh,xdot);
      t = tsav+hh;
      dxdt = (*xdot)(t,xtemp);
      x = rk4(xtemp,dxdt,t,hh,xdot);
      t = tsav+h;
      if(t == tsav) {
         cerr << "Step size too small in routine RKQC\n";
         exit(1);
      }
      xtemp = rk4(xsav,dxsav,tsav,h,xdot);
      errmax = 0.0;
      xtemp = x-xtemp;
      for(i = 1; i <= x.Nrows(); i++) {
         temp = fabs(xtemp(i,1)/xscal(i,1));
         if(errmax < temp) errmax = temp;
      }
      errmax /= eps;
      if(errmax <= 1.0) {
         hdid = h;
         hnext = (errmax > ERRCON ?
            SAFETY*h*exp(PGROW*log(errmax)) : 4.0*h);
         break;
      }
      h = SAFETY*h*exp(PSHRNK*log(errmax));
   }
   x += xtemp*FCOR;
}

#define MAXSTP 10000
#define TINY 1.0e-30

/* integrate the ordinary differential equation xdot from time to
   to time tf using an adaptive step size strategy */
/* adapted from:
   Numerical Recipes in C, The Art of Scientific Computing,
   Press, William H. and Flannery, Brian P. and Teukolsky, Saul A.
   and Vetterling, William T., Cambridge University Press, 1988.
*/
void odeint(ReturnMatrix (*xdot)(Real time, const Matrix & xin),
            Matrix & xo, Real to, Real tf, Real eps, Real h1, Real hmin,
            int & nok, int & nbad,
            RowVector & tout, Matrix & xout, Real dtsav)
{
   int nstp, i;
   Real tsav, t, hnext, hdid, h;
   RowVector tv(1);

   Matrix xscal, x, dxdt;

   tv = to;
   tout = tv;
   xout = xo;
   xscal = xo;
   t = to;
   h = (tf > to) ? fabs(h1) : -fabs(h1);
   nok = (nbad) = 0;
   x = xo;
   tsav = t;
   for(nstp = 1; nstp <= MAXSTP; nstp++){
      dxdt = (*xdot)(t,x);
      for(i = 1; i <= x.Nrows(); i++)
         xscal(i,1) = fabs(x(i,1))+fabs(dxdt(i,1)*h)+TINY;
      if((t+h-tf)*(t+h-to) > 0.0) h = tf-t;
      rkqc(x,dxdt,t,h,eps,xscal,hdid,hnext,xdot);
      if(hdid == h) ++(nok); else ++(nbad);
      if((t-tf)*(tf-to) >= 0.0) {
         xo = x;
         tv = t;
         tout = tout | tv;
         xout = xout | x;
         return;
      }
      if(fabs(t-tsav) > fabs(dtsav)) {
         tv = t;
         tout = tout | tv;
         xout = xout | x;
         tsav = t;
      }
      if(fabs(hnext) <= hmin) {
         cerr << "Step size too small in ODEINT\n";
         cerr << setw(7) << setprecision(3) << (tout & xout).t();
         exit(1);
      }
      h = hnext;
   }
   cerr << "Too many step in routine ODEINT\n";
   exit(1);
}



