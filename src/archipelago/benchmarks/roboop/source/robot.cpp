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

static char rcsid[] = "$Id: robot.cpp,v 1.1 2003/10/17 02:39:04 emery Exp $";

#include "robot.h"

/* global variables */

Real fourbyfourident[] = {1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0};
Real threebythreeident[] ={1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0};

Link::Link(int jt, Real it, Real id, Real ia, Real ial,
      Real mass, Real cmx, Real cmy, Real cmz,
      Real ixx, Real ixy, Real ixz, Real iyy, Real iyz, Real izz):
   joint_type(jt),
   theta(it),
   d(id),
   a(ia),
   alpha(ial),
   R(3,3),
   p(3),
   m(mass),
   r(3),
   I(3,3)
{
   Real ct, st, ca, sa;

   ct = cos(theta);
   st = sin(theta);
   ca = cos(alpha);
   sa = sin(alpha);

   R(1,1) = ct;
   R(2,1) = st;
   R(3,1) = 0.0;
   R(1,2) = -ca*st;
   R(2,2) = ca*ct;
   R(3,2) = sa;
   R(1,3) = sa*st;
   R(2,3) = -sa*ct;
   R(3,3) = ca;

   p(1) = a*ct;
   p(2) = a*st;
   p(3) = d;

   r(1) = cmx;
   r(2) = cmy;
   r(3) = cmz;

   I(1,1) = ixx;
   I(1,2) = I(2,1) = ixy;
   I(1,3) = I(3,1) = ixz;
   I(2,2) = iyy;
   I(2,3) = I(3,2) = iyz;
   I(3,3) = izz;
}

Link::Link(const Link & x)
{
   joint_type = x.joint_type;
   theta = x.theta;
   d = x.d;
   a = x.a;
   alpha = x.alpha;
   R = x.R;
   p = x.p;
   m = x.m;
   r = x.r;
   I = x.I;
}

Link & Link::operator=(const Link & x)
{
   joint_type = x.joint_type;
   theta = x.theta;
   d = x.d;
   a = x.a;
   alpha = x.alpha;
   R = x.R;
   p = x.p;
   m = x.m;
   r = x.r;
   I = x.I;
   return *this;
}

void Link::transform(Real q)
{
   if(joint_type == 0) {
      Real ct, st, ca, sa;
      theta = q;
      ct = cos(theta);
      st = sin(theta);
      ca = R(3,3);
      sa = R(3,2);

      R(1,1) = ct;
      R(2,1) = st;
      R(1,2) = -ca*st;
      R(2,2) = ca*ct;
      R(1,3) = sa*st;
      R(2,3) = -sa*ct;
      p(1) = a*ct;
      p(2) = a*st;
   } else {
      p(3) = d = q;
   }
}

Robot::Robot(int ndof): gravity(3) {
   gravity = 0.0;
   gravity(3) = 9.81; /* default gravity vector */
   dof = ndof;
   links = new Link[dof];
   links = links-1;
}

Robot::Robot(const Matrix & dhinit):
   gravity(3)
{
   int i, ndof;

   gravity = 0.0;
   gravity(3) = 9.81; /* default gravity vector */
   ndof = dhinit.Nrows();
   if(ndof < 1)
      error("Number of degree of freedom must be greater or equal to 1");
   dof = ndof;
   links = new Link[dof];
   links = links-1;
   if (dhinit.Ncols() == 5) {
      for(i = 1; i <= dof; i++) {
         links[i] = Link((int) dhinit(i,1), dhinit(i,2), dhinit(i,3),
                        dhinit(i,4), dhinit(i,5));
      }
   } else if (dhinit.Ncols() == 15) {
      for(i = 1; i <= dof; i++) {
         links[i] = Link((int) dhinit(i,1), dhinit(i,2), dhinit(i,3),
                        dhinit(i,4), dhinit(i,5), dhinit(i,6), dhinit(i,7),
                        dhinit(i,8), dhinit(i,9), dhinit(i,10), dhinit(i,11),
                        dhinit(i,12), dhinit(i,13), dhinit(i,14), dhinit(i,15));
      }
   } else {
      error("?????????");
   }
}


#define MAXSTR 180

Robot::Robot(char * filename) : gravity(3)

{
   char dummy[MAXSTR];
   FILE *fp;
   int ndof, type;
   float theta, d, a, alpha, mass, cx, cy, cz, Ixx, Ixy, Ixz, Iyy, Iyz, Izz;
   int i;


   if((fp = fopen(filename,"r")) == NULL)
      error("Data file not found\n");
   fgets(dummy,MAXSTR,fp);
   fscanf(fp,"%d",&ndof);
   if(ndof < 1)
      error("Number of degree of freedom must be greater or equal to 1");
   dof = ndof;
   links = new Link[dof];
   links = links-1;
   fgets(dummy,MAXSTR,fp);
   fgets(dummy,MAXSTR,fp);
   for(i = 1; i <= dof; i++) {
      fscanf(fp,"%d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
         &type,&theta,&d,&a,&alpha,
         &mass,&cx,&cy,&cz,&Ixx,&Ixy,&Ixz,&Iyy,&Iyz,&Izz);
      fgets(dummy,MAXSTR,fp);
      theta = theta*M_PI/180.0;
      alpha = alpha*M_PI/180.0;
      links[i] = Link(type,theta,d,a,alpha,
                     mass,cx,cy,cz,Ixx,Ixy,Ixz,Iyy,Iyz,Izz);
   }
   fclose(fp);
}


Robot::Robot(const Robot & x)
{
   int i;

   dof = x.dof;
   gravity = x.gravity;
   links = new Link[dof];
   links = links-1;
   for(i = 1; i <= dof; i++) {
      links[i] = x.links[i];
   }
}

Robot::~Robot() {
   links = links+1;
   delete []links;
}

Robot & Robot::operator=(const Robot & x)
{
   int i;

   links = links+1;
   delete []links;
   dof = x.dof;
   gravity = x.gravity;
   links = new Link[dof];
   links = links-1;
   for(i = 1; i <= dof; i++) {
      links[i] = x.links[i];
   }
   return *this;
}

ReturnMatrix Robot::get_q(void)
{
   int i;
   ColumnVector q(dof);

   for(i = 1; i <= dof; i++)
      q(i) = links[i].get_q();
   q.Release(); return q;
}

void Robot::set_q(const Matrix & q)
{
   int i;

   if(q.Nrows() == dof && q.Ncols() == 1) {
      for(i = 1; i <= dof; i++)
         links[i].transform(q(i,1));
   } else if(q.Nrows() == 1 && q.Ncols() == dof) {
      for(i = 1; i <= dof; i++)
         links[i].transform(q(1,i));
   } else error("q has the wrong dimension in set_q()");
}

void Robot::set_q(const ColumnVector & q)
{
   int i;

   if(q.Nrows() == dof) {
      for(i = 1; i <= dof; i++)
         links[i].transform(q(i));
   } else error("q has the wrong dimension in set_q()");
}

void Robot::error(char *msg1, char *msg2)
{
   fprintf(stderr,"\nRobot error: %s %s\n",msg1,msg2);
   exit(1);
}

