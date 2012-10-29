
//#define WANT_STREAM
#define WANT_MATH

#include "include.h"

#include "newmatap.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif


void Print(const Matrix& X);
void Print(const UpperTriangularMatrix& X);
void Print(const DiagonalMatrix& X);
void Print(const SymmetricMatrix& X);
void Print(const LowerTriangularMatrix& X);

void Clean(Matrix&, Real);
void Clean(DiagonalMatrix&, Real);




void trymate()
{
   Tracer et("Fourteenth test of Matrix package");
   Tracer::PrintTrace();

   {
      Tracer et1("Stage 1");
      Matrix A(8,5);
#ifndef ATandT
      Real   a[] =   { 22, 10,  2,  3,  7,
		       14,  7, 10,  0,  8,
		       -1, 13, -1,-11,  3,
		       -3, -2, 13, -2,  4,
			9,  8,  1, -2,  4,
			9,  1, -7,  5, -1,
			2, -6,  6,  5,  1,
			4,  5,  0, -2,  2 };
#else
      Real a[40];
      a[ 0]=22; a[ 1]=10; a[ 2]= 2; a[ 3]= 3; a[ 4]= 7;
      a[ 5]=14; a[ 6]= 7; a[ 7]=10; a[ 8]= 0; a[ 9]= 8;
      a[10]=-1; a[11]=13; a[12]=-1; a[13]=-11;a[14]= 3;
      a[15]=-3; a[16]=-2; a[17]=13; a[18]=-2; a[19]= 4;
      a[20]= 9; a[21]= 8; a[22]= 1; a[23]=-2; a[24]= 4;
      a[25]= 9; a[26]= 1; a[27]=-7; a[28]= 5; a[29]=-1;
      a[30]= 2; a[31]=-6; a[32]= 6; a[33]= 5; a[34]= 1;
      a[35]= 4; a[36]= 5; a[37]= 0; a[38]=-2; a[39]= 2;
#endif
      A << a;
      DiagonalMatrix D; Matrix U; Matrix V;
#ifdef ATandT
      int anc = A.Ncols(); DiagonalMatrix I(anc);     // AT&T 2.1 bug
#else
      DiagonalMatrix I(A.Ncols());
#endif
      I=1.0;
      SymmetricMatrix S1; S1 << A.t() * A;
      SymmetricMatrix S2; S2 << A * A.t();
      Real zero = 0.0; SVD(A+zero,D,U,V);
      DiagonalMatrix D1; SVD(A,D1); Print(DiagonalMatrix(D-D1));
      Matrix SU = U.t() * U - I; Clean(SU,0.000000001); Print(SU);
      Matrix SV = V.t() * V - I; Clean(SV,0.000000001); Print(SV);
      Matrix B = U * D * V.t() - A; Clean(B,0.000000001);Print(B);
      D1=0.0;  SVD(A,D1,A); Print(Matrix(A-U));
      SortDescending(D);
      D(1) -= sqrt(1248); D(2) -= 20; D(3) -= sqrt(384);
      Clean(D,0.000000001); Print(D);
      Jacobi(S1, D, V);
      V = S1 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      SortDescending(D); D(1)-=1248; D(2)-=400; D(3)-=384;
      Clean(D,0.000000001); Print(D);
      Jacobi(S2, D, V);
      V = S2 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      SortDescending(D); D(1)-=1248; D(2)-=400; D(3)-=384;
      Clean(D,0.000000001); Print(D);

      EigenValues(S1, D, V);
      V = S1 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      D(5)-=1248; D(4)-=400; D(3)-=384;
      Clean(D,0.000000001); Print(D);
      EigenValues(S2, D, V);
      V = S2 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      D(8)-=1248; D(7)-=400; D(6)-=384;
      Clean(D,0.000000001); Print(D);

      EigenValues(S1, D);
      D(5)-=1248; D(4)-=400; D(3)-=384;
      Clean(D,0.000000001); Print(D);
      EigenValues(S2, D);
      D(8)-=1248; D(7)-=400; D(6)-=384;
      Clean(D,0.000000001); Print(D);
   }
   {
      Tracer et1("Stage 2");
      Matrix A(20,21);
      int i,j;
      for (i=1; i<=20; i++) for (j=1; j<=21; j++)
      { if (i>j) A(i,j) = 0; else if (i==j) A(i,j) = 21-i; else A(i,j) = -1; }
      A = A.t();
      SymmetricMatrix S1; S1 << A.t() * A;
      SymmetricMatrix S2; S2 << A * A.t();
      DiagonalMatrix D; Matrix U; Matrix V;
#ifdef ATandT
      int anc = A.Ncols(); DiagonalMatrix I(anc);     // AT&T 2.1 bug
#else
      DiagonalMatrix I(A.Ncols());
#endif
      I=1.0;
      SVD(A,D,U,V);
      Matrix SU = U.t() * U - I;    Clean(SU,0.000000001); Print(SU);
      Matrix SV = V.t() * V - I;    Clean(SV,0.000000001); Print(SV);
      Matrix B = U * D * V.t() - A; Clean(B,0.000000001);  Print(B);
      for (i=1; i<=20; i++)  D(i) -= sqrt((22-i)*(21-i));
      Clean(D,0.000000001); Print(D);
      Jacobi(S1, D, V);
      V = S1 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      SortDescending(D);
      for (i=1; i<=20; i++)  D(i) -= (22-i)*(21-i);
      Clean(D,0.000000001); Print(D);
      Jacobi(S2, D, V);
      V = S2 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      SortDescending(D);
      for (i=1; i<=20; i++)  D(i) -= (22-i)*(21-i);
      Clean(D,0.000000001); Print(D);

      EigenValues(S1, D, V);
      V = S1 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      for (i=1; i<=20; i++)  D(i) -= (i+1)*i;
      Clean(D,0.000000001); Print(D);
      EigenValues(S2, D, V);
      V = S2 - V * D * V.t(); Clean(V,0.000000001); Print(V);
      for (i=2; i<=21; i++)  D(i) -= (i-1)*i;
      Clean(D,0.000000001); Print(D);

      EigenValues(S1, D);
      for (i=1; i<=20; i++)  D(i) -= (i+1)*i;
      Clean(D,0.000000001); Print(D);
      EigenValues(S2, D);
      for (i=2; i<=21; i++)  D(i) -= (i-1)*i;
      Clean(D,0.000000001); Print(D);
   }

   {
      Tracer et1("Stage 3");
      Matrix A(30,30);
      int i,j;
      for (i=1; i<=30; i++) for (j=1; j<=30; j++)
      { if (i>j) A(i,j) = 0; else if (i==j) A(i,j) = 1; else A(i,j) = -1; }
      Real d1 = A.LogDeterminant().Value();
      DiagonalMatrix D; Matrix U; Matrix V;
#ifdef ATandT
      int anc = A.Ncols(); DiagonalMatrix I(anc);     // AT&T 2.1 bug
#else
      DiagonalMatrix I(A.Ncols());
#endif
      I=1.0;
      SVD(A,D,U,V);
      Matrix SU = U.t() * U - I; Clean(SU,0.000000001); Print(SU);
      Matrix SV = V.t() * V - I; Clean(SV,0.000000001); Print(SV);
      Real d2 = D.LogDeterminant().Value();
      Matrix B = U * D * V.t() - A; Clean(B,0.000000001); Print(B);
      SortDescending(D);  // Print(D);
      Real d3 = D.LogDeterminant().Value();
      ColumnVector Test(3);
      Test(1) = d1 - 1; Test(2) = d2 - 1; Test(3) = d3 - 1;
      Clean(Test,0.00000001); Print(Test); // only 8 decimal figures
      A.ReSize(2,2);
      Real a = 1.5; Real b = 2; Real c = 2 * (a*a + b*b);
      A << a << b << a << b;
      I.ReSize(2); I=1;
      SVD(A,D,U,V);
      SU = U.t() * U - I; Clean(SU,0.000000001); Print(SU);
      SV = V.t() * V - I; Clean(SV,0.000000001); Print(SV);
      B = U * D * V.t() - A; Clean(B,0.000000001); Print(B);
      D = D*D; SortDescending(D);
      DiagonalMatrix D50(2); D50 << c << 0; D = D - D50;
      Clean(D,0.000000001);
      Print(D);
      A << a << a << b << b;
      SVD(A,D,U,V);
      SU = U.t() * U - I; Clean(SU,0.000000001); Print(SU);
      SV = V.t() * V - I; Clean(SV,0.000000001); Print(SV);
      B = U * D * V.t() - A; Clean(B,0.000000001); Print(B);
      D = D*D; SortDescending(D);
      D = D - D50;
      Clean(D,0.000000001);
      Print(D);
   }

   {
      Tracer et1("Stage 4");

      // test for bug found by Olof Runborg,
      // Department of Numerical Analysis and Computer Science (NADA),
      // KTH, Stockholm

      Matrix A(22,20);

      A = 0;

      int a=1;

      A(a+0,a+2) = 1;     A(a+0,a+18) = -1;
      A(a+1,a+9) = 1;     A(a+1,a+12) = -1;
      A(a+2,a+11) = 1;    A(a+2,a+12) = -1;
      A(a+3,a+10) = 1;    A(a+3,a+19) = -1;
      A(a+4,a+16) = 1;    A(a+4,a+19) = -1;
      A(a+5,a+17) = 1;    A(a+5,a+18) = -1;
      A(a+6,a+10) = 1;    A(a+6,a+4) = -1;
      A(a+7,a+3) = 1;     A(a+7,a+2) = -1;
      A(a+8,a+14) = 1;    A(a+8,a+15) = -1;
      A(a+9,a+13) = 1;    A(a+9,a+16) = -1;
      A(a+10,a+8) = 1;    A(a+10,a+9) = -1;
      A(a+11,a+1) = 1;    A(a+11,a+15) = -1;
      A(a+12,a+16) = 1;   A(a+12,a+4) = -1;
      A(a+13,a+6) = 1;    A(a+13,a+9) = -1;
      A(a+14,a+5) = 1;    A(a+14,a+4) = -1;
      A(a+15,a+0) = 1;    A(a+15,a+1) = -1;
      A(a+16,a+14) = 1;   A(a+16,a+0) = -1;
      A(a+17,a+7) = 1;    A(a+17,a+6) = -1;
      A(a+18,a+13) = 1;   A(a+18,a+5) = -1;
      A(a+19,a+7) = 1;    A(a+19,a+8) = -1;
      A(a+20,a+17) = 1;   A(a+20,a+3) = -1;
      A(a+21,a+6) = 1;    A(a+21,a+11) = -1; 


      Matrix U, V; DiagonalMatrix S;

      SVD(A, S, U, V, true, true);

      DiagonalMatrix D(20); D = 1;

      Matrix tmp = U.t() * U - D;
      Clean(tmp,0.000000001); Print(tmp);

      tmp = V.t() * V - D;
      Clean(tmp,0.000000001); Print(tmp);

      tmp = U * S * V.t() - A ;
      Clean(tmp,0.000000001); Print(tmp);

   }


}
