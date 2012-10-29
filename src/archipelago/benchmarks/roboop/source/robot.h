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

#ifndef __cplusplus
#error Must use C++ for the type Robot
#endif

#if USE_REAP && defined(_WIN32)
#pragma comment (lib, "libreap.lib")
#endif

#if USE_DIEHARD && defined(_WIN32)
#pragma comment (lib, "libdiehard.lib")
#endif

#ifndef ROBOT_H

static char header_rcsid[] = "$Id: robot.h,v 1.2 2005/10/24 03:08:25 emery Exp $";

#include <stdio.h>
#define WANT_STREAM                  /* include.h will get stream fns */
#define WANT_MATH                    /* include.h will get math fns */
                                                                                                 /* newmatap.h will get include.h */

#include "newmatap.h"                /* need matrix applications */

#include "newmatio.h"                /* need matrix output routines */

#ifndef M_PI
#define M_PI 3.141592654
#endif

/* global variables*/

extern Real fourbyfourident[];
extern Real threebythreeident[];

/* vector operation */

ReturnMatrix vec_x_prod(const ColumnVector & x, const ColumnVector & y);

/* numerical analysis tools */

void Runge_Kutta4(ReturnMatrix (*xdot)(Real time, const Matrix & xin),
                  const Matrix & xo, Real to, Real tf, int nsteps,
                  RowVector & tout, Matrix & xout);
void odeint(ReturnMatrix (*xdot)(Real time, const Matrix & xin),
            Matrix & xo, Real to, Real tf, Real eps, Real h1, Real hmin,
            int & nok, int & nbad,
            RowVector & tout, Matrix & xout, Real dtsav);

/* translation */
ReturnMatrix trans(const ColumnVector & a);

/* rotation matrices */
ReturnMatrix rotx(Real alpha);
ReturnMatrix roty(Real beta);
ReturnMatrix rotz(Real gamma);
ReturnMatrix rotk(Real theta, const ColumnVector & k);
ReturnMatrix rpy(const ColumnVector & a);
ReturnMatrix eulzxz(const ColumnVector & a);
ReturnMatrix rotd(Real theta, const ColumnVector & k1, const ColumnVector & k2);


/* inverse on rotation matrices */
ReturnMatrix irotk(const Matrix & R);
ReturnMatrix irpy(const Matrix & R);
ReturnMatrix ieulzxz(const Matrix & R);


/* object classes */
class Link {
   int joint_type; /* joint type: 0=revolute, 1=prismatic */
   Real theta, d, a, alpha; /* D-H parameters */
   public:
   Matrix R; /* orientation matrix */
   ColumnVector p; /* position vector */
   /* constructors */
   Real m; /* mass of the link */
   ColumnVector r; /* position of center of mass */
                   /* w.r.t. link coordinate system */
   Matrix I; /* Inertia matrix w.r.t. center of mass */
             /* and link coordinate system orientation */
   Link(int jt = 0, Real it = 0.0, Real id = 0.0,
         Real ia = 0.0,  Real ial = 0.0, Real mass = 1.0,
         Real cmx = 0.0, Real cmy = 0.0, Real cmz = 0.0,
         Real ixx = 0.0, Real ixy = 0.0, Real ixz = 0.0,
         Real iyy = 0.0, Real iyz = 0.0, Real izz = 0.0);
   Link(const Link & x);
	/* destructor */

   /* operators */
   Link & operator=(const Link & x);
   /* member functions */
   void transform(Real q);
   int get_joint_type(void) { return joint_type; }
   Real get_theta(void) { return theta; }
   Real get_d(void) { return d; }
   Real get_a(void) { return a; }
   Real get_alpha(void) { return alpha; }
   Real get_q(void) {
      if(joint_type == 0) return theta;
      else return d;
   }
};


class Robot {
   public:
   int dof;
   ColumnVector gravity;
   Link *links;
   /* constructors */
   Robot(int ndof=1);
   Robot(const Matrix & dhinit);
   Robot(const Robot & x);
   Robot(char * filename);
   /* destructor */
   ~Robot();
   /* operators */
   Robot & operator=(const Robot & x);
   /* member functions */
   void error(char *msg1, char *msg2 = "");
   void kine(Matrix & Rot, ColumnVector & pos);
   void kine(Matrix & Rot, ColumnVector & pos, int j);
   ReturnMatrix kine(void);
   ReturnMatrix kine(int j);
   ReturnMatrix get_q(void);
   Real get_q(int i) {
      if(i < 1 || i > dof) error("i must be 1 <= i <= dof");
      return links[i].get_q();
   }
   void set_q(const ColumnVector & q);
   void set_q(const Matrix & q);
   void set_q(Real q, int i) {
      if(i < 1 || i > dof) error("i must be 1 <= i <= dof");
         links[i].transform(q);
      }
   ReturnMatrix jacobian(int ref=0);
	void dTdqi(Matrix & dRot, ColumnVector & dp, int i);
   ReturnMatrix dTdqi(int i);
   ReturnMatrix inv_kin(const Matrix & Tobj, int mj = 0);
   ReturnMatrix torque(const ColumnVector & q, const ColumnVector & qp,
                     const ColumnVector & qpp);
   ReturnMatrix acceleration(const ColumnVector & q, const ColumnVector & qp,
                     const ColumnVector & tau);
   ReturnMatrix torque_novelocity(const ColumnVector & q,
                     const ColumnVector & qpp);
   ReturnMatrix inertia(const ColumnVector & q);
   void delta_torque(const ColumnVector & q, const ColumnVector & qp,
                     const ColumnVector & qpp, const ColumnVector & dq,
                     const ColumnVector & dqp, const ColumnVector & dqpp,
                     ColumnVector & torque, ColumnVector & dtorque);
   void dq_torque(const ColumnVector & q, const ColumnVector & qp,
                     const ColumnVector & qpp, const ColumnVector & dq,
                     ColumnVector & torque, ColumnVector & dtorque);
   void dqp_torque(const ColumnVector & q, const ColumnVector & qp,
                     const ColumnVector & dqp, ColumnVector & torque,
                     ColumnVector & dtorque);
   ReturnMatrix dtau_dq(const ColumnVector & q, const ColumnVector & qp,
							const ColumnVector & qpp);
	ReturnMatrix dtau_dqp(const ColumnVector & q, const ColumnVector & qp);
};


/* interface class for gnuplot */

#define LINES       0
#define POINTS      1
#define LINESPOINTS 2
#define IMPULSES    3
#define DOTS        4
#define STEPS       5
#define BOXES       6

#define NCURVESMAX  10  /* maximum number of curves in the same Plot2d */

class Plot2d;

/* object for one curve */
class GNUcurve {
	Matrix xy;     /* n x 2 matrix defining the curve */
	char *clabel;  /* string defining the curve label for the legend */
	int ctype;     /* curve type: lines, points, linespoints, impulses,
							dots, steps, boxes */
	public:
	GNUcurve(const Matrix & data, const char * label = "", int type = LINES);
	GNUcurve(void);
	GNUcurve(const GNUcurve & gnuc);
	GNUcurve & operator=(const GNUcurve & gnuc);
	~GNUcurve(void);
	void dump(void);
	friend class Plot2d;	/* Plot2e need access to private data */
};

/* 2d plot object */
class Plot2d {
	char * title;
	char * xlabel;
	char * ylabel;
   char * gnucommand;
	int ncurves;
	GNUcurve * curves;
	public:
	Plot2d(void);
	~Plot2d(void);
	void dump(void);
	void settitle(const char * t);
	void setxlabel(const char * t);
	void setylabel(const char * t);
	void addcurve(const Matrix & data, const char * label = "", int type = LINES);
	void gnuplot(void);
   void addcommand(const char * gcom);
};

#endif /* ROBOT_H */

