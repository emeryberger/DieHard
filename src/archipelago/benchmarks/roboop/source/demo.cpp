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

static char rcsid[] = "$Id: demo.cpp,v 1.1 2003/10/17 02:39:03 emery Exp $";

#include "robot.h"

void homogen_demo(void);
void kinematics_demo(void);
void dynamics_demo(void);


int main(void)
{
   cout << "=====================================================\n";
   cout << " ROBOOP -- A robotics object oriented package in C++ \n";;
   cout << " DEMO program \n";
   cout << "=====================================================\n";
   cout << "\n";

   homogen_demo();
   kinematics_demo();
   dynamics_demo();

   return(0);
}


void homogen_demo(void)
{
   ColumnVector p1(3), p2(3), p3(3);
   Real ott[] = {1.0,2.0,3.0};
   Real tto[] = {3.0,2.0,1.0};

   cout << "\n";
   cout << "=====================================================\n";
   cout << "Homogeneous transformations\n";
   cout << "=====================================================\n";
   cout << "\n";
   cout << "Translation of [1;2;3]\n";
   p1 << ott;
   cout << setw(7) << setprecision(3) << trans(p1);
   cout << "\n";
   cout << "\n";
   cout << "Rotation of pi/6 around axis X\n";
   cout << setw(7) << setprecision(3) << rotx(M_PI/6);
   cout << "\n";

   cout << "\n";
   cout << "Rotation of pi/8 around axis Y\n";
   cout << setw(7) << setprecision(3) << roty(M_PI/8);
   cout << "\n";

   cout << "\n";
   cout << "Rotation of pi/3 around axis Z\n";
   cout << setw(7) << setprecision(3) << rotz(M_PI/3);
   cout << "\n";

   cout << "\n";
   cout << "Rotation of pi/3 around axis [1;2;3]\n";
   cout << setw(7) << setprecision(3) << rotk(M_PI/3,p1);
   cout << "\n";

   cout << "\n";
   cout << "Rotation of pi/6 around line [1;2;3] -- [3;2;1]\n";
   p2 << tto;
   cout << setw(7) << setprecision(3) << rotd(M_PI/3,p1,p2);
   cout << "\n";

   cout << "\n";
   cout << "Roll Pitch Yaw Rotation [1;2;3]\n";
   cout << setw(7) << setprecision(3) << rpy(p1);
   cout << "\n";

   cout << "\n";
   cout << "Euler ZXZ Rotation [3;2;1]\n";
   cout << setw(7) << setprecision(3) << eulzxz(p2);
   cout << "\n";

   cout << "\n";
   cout << "Inverse of Rotation around axis\n";
   cout << setw(7) << setprecision(3) << irotk(rotk(M_PI/3,p1));
   cout << "\n";

   cout << "\n";
   cout << "Inverse of Roll Pitch Yaw Rotation\n";
   cout << setw(7) << setprecision(3) << irpy(rpy(p1));
   cout << "\n";

   cout << "\n";
   cout << "Inverse of Euler ZXZ Rotation\n";
   cout << setw(7) << setprecision(3) << ieulzxz(eulzxz(p2));
   cout << "\n";

   cout << "\n";
   cout << "Cross product between [1;2;3] and [3;2;1]\n";
   cout << setw(7) << setprecision(3) << vec_x_prod(p1,p2);
   cout << "\n";

}

Real RR_data[] =
  {0.0, 0.0, 0.0, 1.0, 0.0, 2.0,-0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1666666, 0.0, 0.1666666,
   0.0, 0.0, 0.0, 1.0, 0.0, 1.0,-0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0833333, 0.0, 0.0833333};
Real RP_data[] =
  {0.0, 0.0, 0.0, 0.0, -M_PI/2.0, 2.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0,
   1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,-1.0, 0.0833333, 0.0, 0.0, 0.0833333, 0.0, 0.0833333};
Real PUMA560_data[] =
  {0.0, 0.0, 0.0, 0.0, M_PI/2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.35, 0.0, 0.0,
   0.0, 0.0, 0.0, 0.4318, 0.0, 17.4, -0.3638, 0.006, 0.2275, 0.13, 0.0, 0.0, 0.524, 0.0, 0.539,
   0.0, 0.0, 0.15005, 0.0203, -M_PI/2.0, 4.8, -0.0203, -0.0141, 0.07, 0.066, 0.0, 0.0, 0.086, 0.0, 0.0125,
   0.0, 0.0, 0.4318, 0.0, M_PI/2.0, 0.82, 0.0, 0.019, 0.0, 0.0018, 0.0, 0.0, 0.0013, 0.0, 0.0018,
   0.0, 0.0, 0.0, 0.0, -M_PI/2.0, 0.34, 0.0, 0.0, 0.0, 0.0003, 0.0, 0.0, 0.0004, 0.0, 0.0003,
   0.0, 0.0, 0.0, 0.0, 0.0, 0.09, 0.0, 0.0, 0.032, 0.00015, 0.0, 0.0, 0.00015, 0.0, 0.00004};

Real STANFORD_data[] =
  {0.0, 0.0, 0.4120, 0.0, -M_PI/2, 9.29, 0.0, 0.0175, -0.1105, 0.276, 0.0, 0, 0.255, 0.0, 0.071,
   0.0, 0.0, 0.1540, 0.0, M_PI/2.0, 5.01, 0.0, -1.054, 0.0, 0.108, 0.0, 0.0, 0.018, 0.0, 0.1,
   1.0, -M_PI/2.0, 0.0, 0.0, 0.0, 4.25, 0.0, 0.0, -6.447, 2.51, 0.0, 0.0, 2.51, 0.0, 0.006,
   0.0, 0.0, 0.0, 0.0, -M_PI/2.0, 1.08, 0.0, 0.092, -0.054, 0.002, 0.0, 0.0, 0.001, 0.0, 0.001,
   0.0, 0.0, 0.0, 0.0,  M_PI/2.0, 0.63, 0.0, 0.0, 0.566, 0.003, 0.0, 0.0, 0.003, 0.0, 0.0004,
   0.0, 0.0, 0.2630, 0.0, 0.0, 0.51, 0.0, 0.0, 1.5540, 0.013, 0.0, 0.0, 0.013, 0.0, 0.0003};

Robot robot;
Matrix K;
ColumnVector q0;

ReturnMatrix xdot(Real t, const Matrix & x)
{
   int ndof;
   ColumnVector q, qp, qpp; /* position, velocities and accelerations */
   ColumnVector tau, dx;              /* torque and state space error */
   Matrix xd;

   ndof = robot.dof;                             /* get the # of dof  */
   q = x.SubMatrix(1,ndof,1,1);               /* position, velocities */
   qp = x.SubMatrix(ndof+1,2*ndof,1,1);          /* from state vector */
   qpp = ColumnVector(ndof);
   qpp = 0.0;                               /* set the vector to zero */
   tau = robot.torque(q0,qpp,qpp);      /* compute the gravity torque */
   dx = (q-q0) & qp;       /* compute dx using vertical concatenation */
   tau = tau - K*dx;                           /* feedback correction */
   qpp = robot.acceleration(q, qp, tau);              /* acceleration */
   xd = qp & qpp;                          /* state vector derivative */

   xd.Release(); return xd;
}

void kinematics_demo(void)
{
   Matrix initrob(2,15), Tobj;
   ColumnVector qs, qr;
   int i;

   cout << "\n";
   cout << "=====================================================\n";
   cout << "Robot RR kinematics\n";
   cout << "=====================================================\n";
   initrob << RR_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "   type     theta      d        a      alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,2,1,5);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   qr = ColumnVector(2);
   qr = M_PI/4.0;
   robot.set_q(qr);
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   cout << "Robot Jacobian w/r to base frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian();
   cout << "\n";
   cout << "Robot Jacobian w/r to tool frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian(qr.Nrows());
   cout << "\n";
   for (i = 1; i <= qr.Nrows(); i++) {
      cout << "Robot position partial derivative with respect to joint " << i << " \n";
      cout << setw(7) << setprecision(3) << robot.dTdqi(i);
      cout << "\n";
   }
   Tobj = robot.kine();
   qs = ColumnVector(2);
   qs = M_PI/16.0;
   cout << "Robot inverse kinematics\n";
   cout << "  q start  q final  q real\n";
   cout << setw(7) << setprecision(3) << (qs | robot.inv_kin(Tobj) | qr);
   cout << "\n";
   cout << "\n";

   cout << "=====================================================\n";
   cout << "Robot RP kinematics\n";
   cout << "=====================================================\n";
   initrob << RP_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,2,1,5);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   robot.set_q(M_PI/4.0,1);
   robot.set_q(4.0,2);
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   cout << "Robot Jacobian w/r to base frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian();
   cout << "\n";
   qr = robot.get_q();
   cout << "Robot Jacobian w/r to tool frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian(qr.Nrows());
   cout << "\n";
   for (i = 1; i <= qr.Nrows(); i++) {
      cout << "Robot position partial derivative with respect to joint " << i << " \n";
      cout << setw(7) << setprecision(3) << robot.dTdqi(i);
      cout << "\n";
   }
   Tobj = robot.kine();
   robot.set_q(M_PI/16.0,1);
   robot.set_q(1.0,2);
   qs = robot.get_q();
   cout << "Robot inverse kinematics\n";
   cout << " q start q final q real\n";
   cout << setw(7) << setprecision(3) << (qs | robot.inv_kin(Tobj) | qr);
   cout << "\n";
   cout << "\n";

   cout << "=====================================================\n";
   cout << "Robot PUMA560 kinematics\n";
   cout << "=====================================================\n";
   initrob = Matrix(6,15);
   initrob << PUMA560_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,6,1,5);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   qr = ColumnVector(6);
   qr = M_PI/4.0;
   robot.set_q(qr);
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   cout << "Robot Jacobian w/r to base frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian();
   cout << "\n";
   cout << "Robot Jacobian w/r to tool frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian(qr.Nrows());
   cout << "\n";
   for (i = 1; i <= qr.Nrows(); i++) {
      cout << "Robot position partial derivative with respect to joint " << i << " \n";
      cout << setw(7) << setprecision(3) << robot.dTdqi(i);
      cout << "\n";
   }
   Tobj = robot.kine();
   qs = ColumnVector(6);
   qs = 1.0;
   qs(1) = M_PI/16.0;
   robot.set_q(qs);
   cout << "Robot inverse kinematics\n";
   cout << " q start q final q real\n";
   cout << setw(7) << setprecision(3) << (qs | robot.inv_kin(Tobj) | qr);
   cout << "\n";
   cout << "\n";

   cout << "=====================================================\n";
   cout << "Robot STANFORD kinematics\n";
   cout << "=====================================================\n";
   initrob = Matrix(6,15);
   initrob << STANFORD_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,6,1,5);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   qr = ColumnVector(6);
   qr = M_PI/4.0;
   robot.set_q(qr);
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot position\n";
   cout << setw(7) << setprecision(3) << robot.kine();
   cout << "\n";
   cout << "Robot Jacobian w/r to base frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian();
   cout << "\n";
   cout << "Robot Jacobian w/r to tool frame\n";
   cout << setw(7) << setprecision(3) << robot.jacobian(qr.Nrows());
   cout << "\n";
   for (i = 1; i <= qr.Nrows(); i++) {
      cout << "Robot position partial derivative with respect to joint " << i << " \n";
      cout << setw(7) << setprecision(3) << robot.dTdqi(i);
      cout << "\n";
   }
   Tobj = robot.kine();
   qs = ColumnVector(6);
   qs = 1.0;
   qs(1) = M_PI/16.0;
   robot.set_q(qs);
   cout << "Robot inverse kinematics\n";
   cout << " q start q final q real\n";
   cout << setw(7) << setprecision(3) << (qs | robot.inv_kin(Tobj) | qr);
   cout << "\n";
   cout << "\n";
}

void dynamics_demo(void)
{
   int nok, nbad;
   Matrix initrob(2,15), Tobj, xout;
   ColumnVector qs, qr;
   RowVector tout;

   cout << "\n";
   cout << "=====================================================\n";
   cout << "Robot RR dynamics\n";
   cout << "=====================================================\n";
   initrob << RR_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,2,1,5);
   cout << "\n";
   cout << "Robot D-H inertial parameters\n";
   cout << "  mass     cx       cy      cz     Ixx     Ixy     Ixz     Iyy     Iyz     Izz\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,2,6,15);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot Inertia matrix\n";
   cout << setw(7) << setprecision(3) << robot.inertia(robot.get_q());
   cout << "\n";
   K = Matrix(2,4);
   K = 0.0;
   K(1,1) = K(2,2) = 25.0;      /* K = [25I 7.071I ] */
   K(1,3) = K(2,4) = 7.071;
   cout << "Feedback gain matrix K\n";
   cout << setw(7) << setprecision(3) << K;
   cout << "\n";
   q0 = ColumnVector(2);
   q0 = M_PI/4.0;
   qs = ColumnVector(4);
   qs = 0.0;
   cout << "  time     q1       q2      q1p    q2p\n";
/*   Runge_Kutta4(xdot, qs, 0.0, 4.0, 160, tout, xout);
replaced by adaptive step size */
   odeint(xdot, qs, 0.0, 4.0, 1e-4,0.1, 1e-12, nok, nbad, tout, xout, 0.05);
   cout << setw(7) << setprecision(3) << (tout & xout).t();
   cout << "\n";
   cout << "\n";
#if 0
   Plot2d plotposition, plotvelocity;
   plotposition.settitle("Robot joints position");
   plotposition.setxlabel("time (sec)");
   plotposition.setylabel("q1 and q2 (rad)");
   plotposition.addcurve((tout & xout.SubMatrix(1,1,1,xout.Ncols())).t(),
                        "q1",POINTS);
   plotposition.addcurve((tout & xout.SubMatrix(2,2,1,xout.Ncols())).t(),
                        "q2",POINTS);
   /* uncomment the following two lines to obtain a
      ps file of the graph */
/*   plotposition.addcommand("set term post");
   plotposition.addcommand("set output \"demo.ps\"");*/
   plotposition.gnuplot();

   plotvelocity.settitle("Robot joints velocity");
   plotvelocity.setxlabel("time (sec)");
   plotvelocity.setylabel("qp1 and qp2 (rad/s)");
   plotvelocity.addcurve((tout & xout.SubMatrix(3,3,1,xout.Ncols())).t(),
                        "qp1",POINTS);
   plotvelocity.addcurve((tout & xout.SubMatrix(4,4,1,xout.Ncols())).t(),
                        "qp2",POINTS);
   plotvelocity.gnuplot();
#endif

   cout << "=====================================================\n";
   cout << "Robot RP dynamics\n";
   cout << "=====================================================\n";
   initrob << RP_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,2,1,5);
   cout << "\n";
   cout << "Robot D-H inertial parameters\n";
   cout << "  mass     cx       cy      cz     Ixx     Ixy     Ixz     Iyy     Iyz     Izz\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,2,6,15);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot Inertia matrix\n";
   cout << setw(7) << setprecision(3) << robot.inertia(robot.get_q());
   cout << "\n";
   cout << "\n";

   cout << "=====================================================\n";
   cout << "Robot PUMA560 dynamics\n";
   cout << "=====================================================\n";
   initrob = Matrix(6,15);
   initrob << PUMA560_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,6,1,5);
   cout << "\n";
   cout << "Robot D-H inertial parameters\n";
   cout << "  mass     cx       cy      cz     Ixx     Ixy     Ixz     Iyy     Iyz     Izz\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,6,6,15);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot Inertia matrix\n";
   cout << setw(8) << setprecision(4) << robot.inertia(robot.get_q());
   cout << "\n";
   qs = ColumnVector(6);
   qr = ColumnVector(6);
   qs =0.0;
   qr =0.0;
   cout << "Robot Torque\n";
   cout << setw(8) << setprecision(4) << robot.torque(robot.get_q(),qs,qr);
   cout << "\n";
   cout << "Robot acceleration\n";
   cout << setw(8) << setprecision(4) << robot.acceleration(robot.get_q(),qs,qr);
   cout << "\n";

   cout << "\n";
   cout << "=====================================================\n";
   cout << "Robot STANFORD dynamics\n";
   cout << "=====================================================\n";
   initrob = Matrix(6,15);
   initrob << STANFORD_data;
   robot = Robot(initrob);
   cout << "\n";
   cout << "Robot D-H parameters\n";
   cout << "  type    theta     d       a     alpha\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,6,1,5);
   cout << "\n";
   cout << "Robot D-H inertial parameters\n";
   cout << "  mass     cx       cy      cz     Ixx     Ixy     Ixz     Iyy     Iyz     Izz\n";
   cout << setw(7) << setprecision(3) << initrob.SubMatrix(1,6,6,15);
   cout << "\n";
   cout << "Robot joints variables\n";
   cout << setw(7) << setprecision(3) << robot.get_q();
   cout << "\n";
   cout << "Robot Inertia matrix\n";
   cout << setw(7) << setprecision(3) << robot.inertia(robot.get_q());
   cout << "\n";
}
