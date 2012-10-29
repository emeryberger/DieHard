#include "include.h"
#include "newmatap.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif


/**************************** test program ******************************/

void Print(const Matrix& X);
void Print(const UpperTriangularMatrix& X);
void Print(const DiagonalMatrix& X);
void Print(const SymmetricMatrix& X);
void Print(const LowerTriangularMatrix& X);

void Clean(Matrix&, Real);
void Clean(DiagonalMatrix&, Real);

void trymat9()
{
   Tracer et("Ninth test of Matrix package");
   Tracer::PrintTrace();


   int i; int j;
   Matrix A(7,7); Matrix X(7,3);
   for (i=1;i<=7;i++) for (j=1;j<=7;j++) A(i,j)=i*i+j+((i==j) ? 1 : 0);
   for (i=1;i<=7;i++) for (j=1;j<=3;j++) X(i,j)=i-j;
   Matrix B = A.i(); DiagonalMatrix D(7); D=1.0;
   {
      Tracer et1("Stage 1");
      Matrix Q = B*A-D; Clean(Q, 0.000000001); Print(Q);
      Q=A; Q = Q.i() * X; Q = A*Q - X; Clean(Q, 0.000000001); Print(Q);
      Q=X; Q = A.i() * Q; Q = A*Q - X; Clean(Q, 0.000000001); Print(Q);
   }
   for (i=1;i<=7;i++) D(i,i)=i*i+1;
   DiagonalMatrix E(3); for (i=1;i<=3;i++) E(i,i)=i+23;
   {
      Tracer et1("Stage 2");
      Matrix DXE = D.i() * X * E;
      DXE = E.i() * DXE.t() * D - X.t(); Clean(DXE, 0.00000001); Print(DXE); 
      E=D; for (i=1;i<=7;i++) E(i,i)=i*3+1;
   }
   DiagonalMatrix F=D;
   {
      Tracer et1("Stage 3");
      F=E.i()*F; F=F*E-D; Clean(F,0.00000001); Print(F);
      F=E.i()*D; F=F*E-D; Clean(F,0.00000001); Print(F);
   }
   {
      Tracer et1("Stage 4");
      F=E; F=F.i()*D; F=F*E-D; Clean(F,0.00000001); Print(F);
   }
   {
      Tracer et1("Stage 5");
      // testing equal
      ColumnVector A(17), B(17);
      Matrix X(3,3);
      X << 3 << 5  << 7 << 5 << 8 << 2 << 7 << 2 << 9;
      SymmetricMatrix S; S << X;
      B(1) = S == X;         A(1) = true;
      B(2) = S == (X+1);     A(2) = false;
      B(3) = (S+2) == (X+2); A(3) = true;
      Matrix Y = X;
      B(4) = X == Y;         A(4) = true;
      B(5) = (X*2) == (Y*2); A(5) = true;
      Y(3,3) = 10;
      B(6) = X == Y;         A(6) = false;
      B(7) = (X*2) == (Y*2); A(7) = false;
      B(8) = S == Y;         A(8) = false;
      B(9) = S == S;         A(9) = true;
      Matrix Z = X.SubMatrix(1,2,2,3);
      B(10) = X == Z;        A(10) = false;
      GenericMatrix GS = S;
      GenericMatrix GX = X;
      GenericMatrix GY = Y;
      B(11) = GS == GX;      A(11) = true;
      B(12) = GS == GY;      A(12) = false;
      CroutMatrix CS = S;
      CroutMatrix CX = X;
      CroutMatrix CY = Y;
      B(13) = CS == CX;      A(13) = true;
      B(14) = CS == CY;      A(14) = false;
      B(15) = X == CX;       A(15) = false;
      B(16) = X == A;        A(16) = false;
      B(17) = X == (X | X);  A(17) = false;
      A = A - B; Print(A);
   }
   {
      Tracer et1("Stage 6");
      // testing equal
      ColumnVector A(21), B(21);
      BandMatrix X(6,2,1);
      X(1,1)=23; X(1,2)=21;
      X(2,1)=12; X(2,2)=17; X(2,3)=45;
      X(3,1)=35; X(3,2)=19; X(3,3)=24; X(3,4)=29;
                 X(4,2)=17; X(4,3)=11; X(4,4)=19; X(4,5)=35;
                            X(5,3)=10; X(5,4)=44; X(5,5)=23; X(5,6)=31;
                                       X(6,4)=49; X(6,5)=41; X(6,6)=17;
      BandMatrix U(6,2,3); U = 0.0; U.Inject(X);
      B(1) = U == X;         A(1) = true;
      B(2) = U == (X*3);     A(2) = false;
      B(3) = (U*5) == (X*5); A(3) = true;
      Matrix Y = X;
      B(4) = X == Y;         A(4) = true;
      B(5) = (X*2) == (Y*2); A(5) = true;
      Y(6,6) = 10;
      B(6) = X == Y;         A(6) = false;
      B(7) = (X*2) == (Y*2); A(7) = false;
      B(8) = U == Y;         A(8) = false;
      B(9) = U == U;         A(9) = true;
      Matrix Z = X.SubMatrix(1,2,2,3);
      B(10) = X == Z;        A(10) = false;
      GenericMatrix GU = U;
      GenericMatrix GX = X;
      GenericMatrix GY = Y;
      B(11) = GU == GX;      A(11) = true;
      B(12) = GU == GY;      A(12) = false;
      X = X + X.t(); U = U + U.t();
      SymmetricBandMatrix S(6,2); S.Inject(X);
      Matrix D = S-X; Print(D);
      BandLUMatrix BS = S;
      BandLUMatrix BX = X;
      BandLUMatrix BU = U;
      CroutMatrix CX = X;
      B(13) = BS == BX;      A(13) = true;
      B(14) = BX == BU;      A(14) = false;
      B(15) = X == BX;       A(15) = false;
      B(16) = X != BX;       A(16) = true;
      B(17) = BX != BS;      A(17) = false;
      B(18) = (2*X) != (X*2);A(18) = false;
      B(19) = (X*2) != (X+2);A(19) = true;
      B(20) = BX == CX;      A(20) = false;
      B(21) = CX == BX;      A(21) = false;
      A = A - B; Print(A);
      DiagonalMatrix I(6); I=1.0;
      D = BS.i() * X - I;  Clean(D,0.00000001); Print(D);
      D = BX.i() * X - I;  Clean(D,0.00000001); Print(D);
      D = BU.i() * X - I;  Clean(D,0.00000001); Print(D);
   }
   {
      Tracer et1("Stage 7");
      // testing equal
      ColumnVector A(12), B(12);
      BandMatrix X(6,2,1);
      X(1,1)=23; X(1,2)=21;
      X(2,1)=12; X(2,2)=17; X(2,3)=45;
      X(3,1)=35; X(3,2)=19; X(3,3)=24; X(3,4)=29;
                 X(4,2)=17; X(4,3)=11; X(4,4)=19; X(4,5)=35;
                            X(5,3)=10; X(5,4)=44; X(5,5)=23; X(5,6)=31;
                                       X(6,4)=49; X(6,5)=41; X(6,6)=17;
      Matrix Y = X;
      LinearEquationSolver LX = X;
      LinearEquationSolver LY = Y;
      CroutMatrix CX = X;
      CroutMatrix CY = Y;
      BandLUMatrix BX = X;
      B(1) = LX == CX;       A(1) = false;
      B(2) = LY == CY;       A(2) = true;
      B(3) = X == Y;         A(3) = true;
      B(4) = BX == LX;       A(4) = true;
      B(5) = CX == CY;       A(5) = true;
      B(6) = LX == LY;       A(6) = false;
      B(7) = BX == BX;       A(7) = true;
      B(8) = CX == CX;       A(8) = true;
      B(9) = LX == LX;       A(9) = true;
      B(10) = LY == LY;      A(10) = true;
      CroutMatrix CX1 = X.SubMatrix(1,4,1,4);
      B(11) = CX == CX1;     A(11) = false;
      BandLUMatrix BX1 = X.SubMatrix(1,4,1,4);
      B(12) = BX == BX1;     A(12) = false;
      A = A - B; Print(A);
      DiagonalMatrix I(6); I=1.0; Matrix D;
      D = LX.i() * X - I;  Clean(D,0.00000001); Print(D);
      D = LY.i() * X - I;  Clean(D,0.00000001); Print(D);
   }
//   cout << "\nEnd of ninth test\n";
}

