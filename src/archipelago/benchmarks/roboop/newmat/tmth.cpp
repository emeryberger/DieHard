
//#define WANT_STREAM

#include "include.h"

#include "newmatap.h"
//#include "newmatio.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif


void Print(const Matrix& X);
void Print(const UpperTriangularMatrix& X);
void Print(const DiagonalMatrix& X);
void Print(const SymmetricMatrix& X);
void Print(const LowerTriangularMatrix& X);

void Clean(Matrix&, Real);




void trymath()
{
//   cout << "\nSeventeenth test of Matrix package\n";
//   cout << "\n";
   Tracer et("Seventeenth test of Matrix package");
   Tracer::PrintTrace();


   {
      Tracer et1("Stage 1");
      int i, j;
      BandMatrix B(8,3,1);
      for (i=1; i<=8; i++) for (j=-3; j<=1; j++)
	 { int k = i+j; if (k>0 && k<=8) B(i,k) = i + k/64.0; }

      DiagonalMatrix I(8); I = 1; BandMatrix B1; B1 = I;
      UpperTriangularMatrix UT = I; Print(Matrix(B1-UT));
      Print(Matrix(B * B - B * 2 + I - (B - I) * (B - I)));
      Matrix A = B; BandMatrix C; C = B;
      Print(Matrix(B * A - C * 2 + I - (A - I) * (B - I)));

      C.ReSize(8,2,2); C = 0.0; C.Inject(B);
      Matrix X = A.t();
      B1.ReSize(8,2,2); B1.Inject(X); Print(Matrix(C.t()-B1));

      Matrix BI = B.i(); A = A.i()-BI; Clean(A,0.000000001); Print(A);
      BandLUMatrix BLU = B.t();
      BI = BLU.i(); A = X.i()-BI; Clean(A,0.000000001); Print(A);
      X.ReSize(1,1);
      X(1,1) = BLU.LogDeterminant().Value()-B.LogDeterminant().Value();
      Clean(X,0.000000001); Print(X);

      UpperBandMatrix U; U << B; LowerBandMatrix L; L << B;
      DiagonalMatrix D; D << B;
      Print( Matrix(L + (U - D - B)) );



      for (i=1; i<=8; i++)  A.Column(i) << B.Column(i);
      Print(Matrix(A-B));
   }
   {
      Tracer et1("Stage 2");
      BandMatrix A(7,2,2);
      int i,j;
      for (i=1; i<=7; i++) for (j=1; j<=7; j++)
      {
	 int k=i-j; if (k<0) k = -k;
	 if (k==0) A(i,j)=6;
	 else if (k==1) A(i,j) = -4;
	 else if (k==2) A(i,j) = 1;
	 A(1,1) = A(7,7) = 5;
      }
      DiagonalMatrix D(7); D = 0.0; A = A - D;
      BandLUMatrix B(A); Matrix M = A;
      ColumnVector V(3);
      V(1) = LogDeterminant(B).Value();
      V(2) = LogDeterminant(A).Value();
      V(3) = LogDeterminant(M).Value();
      V = V / 64 - 1; Clean(V,0.000000001); Print(V);
      ColumnVector X(7);

#ifdef ATandT
      Real a[7];
      // the previous statement causes a core dump in tmti.cpp
      // on the HP9000 - seems very strange. Possibly the exception
      // mechanism is failing to track the stack correctly. If you get
      // this problem replace by the following statement.
//      Real* a = new Real [7]; if (!a) exit(1);
      a[0]=1; a[1]=2; a[2]=3; a[3]=4; a[4]=5; a[5]=6; a[6]=7;
#else
      Real a[] = {1,2,3,4,5,6,7};
#endif
      X << a;
#ifdef ATandT
      delete [] a;
#endif
      M = (M.i()*X).t() - (B.i()*X).t() * 2.0 + (A.i()*X).t();
      Clean(M,0.000000001); Print(M);


      BandMatrix P(80,2,5); ColumnVector CX(80);
      for (i=1; i<=80; i++) for (j=1; j<=80; j++)
      { int d = i-j; if (d<=2 && d>=-5) P(i,j) = i + j/100.0; }
      for (i=1; i<=80; i++)  CX(i) = i*100.0;
      Matrix MP = P;
      ColumnVector V1 = P.i() * CX; ColumnVector V2 = MP.i() * CX;
      V = V1 - V2; Clean(V,0.000000001); Print(V);

      V1 = P * V1; V2 = MP * V2; V = V1 - V2; Clean(V,0.000000001); Print(V);
      RowVector XX(1);
      XX = LogDeterminant(P).Value() / LogDeterminant(MP).Value() - 1.0;
      Clean(XX,0.000000001); Print(XX);

      LowerBandMatrix LP(80,5);
      for (i=1; i<=80; i++) for (j=1; j<=80; j++)
      { int d = i-j; if (d<=5 && d>=0) LP(i,j) = i + j/100.0; }
      MP = LP;
      XX.ReSize(4);
      XX(1) = LogDeterminant(LP).Value();
      XX(2) = LogDeterminant(MP).Value();
      V1 = LP.i() * CX; V2 = MP.i() * CX;
      V = V1 - V2; Clean(V,0.000000001); Print(V);

      UpperBandMatrix UP(80,4);
      for (i=1; i<=80; i++) for (j=1; j<=80; j++)
      { int d = i-j; if (d<=0 && d>=-4) UP(i,j) = i + j/100.0; }
      MP = UP;
      XX(3) = LogDeterminant(UP).Value();
      XX(4) = LogDeterminant(MP).Value();
      V1 = UP.i() * CX; V2 = MP.i() * CX;
      V = V1 - V2; Clean(V,0.000000001); Print(V);
      XX = XX / SumAbsoluteValue(XX) - .25; Clean(XX,0.000000001); Print(XX);
   }

   {
      Tracer et1("Stage 3");
      SymmetricBandMatrix SA(8,5);
      int i,j;
      for (i=1; i<=8; i++) for (j=1; j<=8; j++)
	 if (i-j<=5 && 0<=i-j) SA(i,j) =i + j/128.0;
      DiagonalMatrix D(8); D = 10; SA = SA + D;

      Matrix MA1(8,8); Matrix MA2(8,8);
      for (i=1; i<=8; i++)
	 { MA1.Column(i) << SA.Column(i); MA2.Row(i) << SA.Row(i); }
      Print(Matrix(MA1-MA2));

      D = 10; SA = SA.t() + D; MA1 = MA1 + D;
      Print(Matrix(MA1-SA));

      UpperBandMatrix UB(8,3); LowerBandMatrix LB(8,4);
      D << SA; UB << SA; LB << SA;
      SA = SA * 5.0; D = D * 5.0; LB = LB * 5.0; UB = UB * 5.0;
      BandMatrix B = LB - D + UB - SA; Print(Matrix(B));

      SymmetricBandMatrix A(7,2); A = 100.0;
      for (i=1; i<=7; i++) for (j=1; j<=7; j++)
      {
	 int k=i-j;
	 if (k==0) A(i,j)=6;
	 else if (k==1) A(i,j) = -4;
	 else if (k==2) A(i,j) = 1;
	 A(1,1) = A(7,7) = 5;
      }
      BandLUMatrix C(A); Matrix M = A;
      ColumnVector X(8);
      X(1) = LogDeterminant(C).Value() - 64;
      X(2) = LogDeterminant(A).Value() - 64;
      X(3) = LogDeterminant(M).Value() - 64;
      X(4) = SumSquare(M) - SumSquare(A);
      X(5) = SumAbsoluteValue(M) - SumAbsoluteValue(A);
      X(6) = MaximumAbsoluteValue(M) - MaximumAbsoluteValue(A);
      X(7) = Trace(M) - Trace(A);
      X(8) = Sum(M) - Sum(A);
      Clean(X,0.000000001); Print(X);

#ifdef ATandT
      Real a[7]; a[0]=1; a[1]=2; a[2]=3; a[3]=4; a[4]=5; a[5]=6; a[6]=7;
#else
      Real a[] = {1,2,3,4,5,6,7};
#endif
      X.ReSize(7);
      X << a;
      X = M.i()*X - C.i()*X * 2 + A.i()*X;
      Clean(X,0.000000001); Print(X);


      LB << A; UB << A; D << A;
      BandMatrix XA = LB + (UB - D);
      Print(Matrix(XA - A));

      for (i=1; i<=7; i++) for (j=1; j<=7; j++)
      {
	 int k=i-j;
	 if (k==0) A(i,j)=6;
	 else if (k==1) A(i,j) = -4;
	 else if (k==2) A(i,j) = 1;
	 A(1,1) = A(7,7) = 5;
      }
      D = 1;

      M = LB.i() * LB - D; Clean(M,0.000000001); Print(M);
      M = UB.i() * UB - D; Clean(M,0.000000001); Print(M);
      M = XA.i() * XA - D; Clean(M,0.000000001); Print(M);
//      Matrix MUB = UB; Matrix MLB = LB;
//      M = LB.i() * UB - LB.i() * MUB; Clean(M,0.000000001); Print(M);
//      M = UB.i() * LB - UB.i() * MLB; Clean(M,0.000000001); Print(M);
      M = LB.i() * UB - LB.i() * Matrix(UB); Clean(M,0.000000001); Print(M);
      M = UB.i() * LB - UB.i() * Matrix(LB); Clean(M,0.000000001); Print(M);
   }


//   cout << "\nEnd of Seventeenth test\n";
}
