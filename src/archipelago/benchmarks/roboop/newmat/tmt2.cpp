
//#define WANT_STREAM


#include "include.h"

#include "newmat.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif


/**************************** test program ******************************/

void Print(const Matrix& X);
void Print(const UpperTriangularMatrix& X);
void Print(const DiagonalMatrix& X);
void Print(const SymmetricMatrix& X);
void Print(const LowerTriangularMatrix& X);


void trymat2()
{
//   cout << "\nSecond test of Matrix package\n\n";
   Tracer et("Second test of Matrix package");
   Tracer::PrintTrace();

   int i,j;
   Matrix M(3,5);
   for (i=1; i<=3; i++) for (j=1; j<=5; j++) M(i,j) = 100*i + j;
   Matrix X(8,10);
   for (i=1; i<=8; i++) for (j=1; j<=10; j++) X(i,j) = 1000*i + 10*j;
   Matrix Y = X; Matrix Z = X;
   { X.SubMatrix(2,4,3,7) << M; }
   for (i=1; i<=3; i++) for (j=1; j<=5; j++) Y(i+1,j+2) = 100*i + j;
   Print(Matrix(X-Y));


   Real a[15]; Real* r = a;
   for (i=1; i<=3; i++) for (j=1; j<=5; j++) *r++ = 100*i + j;
   { Z.SubMatrix(2,4,3,7) << a; }
   Print(Matrix(Z-Y));

   { M=33; X.SubMatrix(2,4,3,7) << M; }
   { Z.SubMatrix(2,4,3,7) = 33; }
   Print(Matrix(Z-X));

   for (i=1; i<=8; i++) for (j=1; j<=10; j++) X(i,j) = 1000*i + 10*j;
   Y = X;
   UpperTriangularMatrix U(5);
   for (i=1; i<=5; i++) for (j=i; j<=5; j++) U(i,j) = 100*i + j;
   { X.SubMatrix(3,7,5,9) << U; }
   for (i=1; i<=5; i++) for (j=i; j<=5; j++) Y(i+2,j+4) = 100*i + j;
   for (i=1; i<=5; i++) for (j=1; j<i; j++) Y(i+2,j+4) = 0.0;
   Print(Matrix(X-Y));
   for (i=1; i<=8; i++) for (j=1; j<=10; j++) X(i,j) = 1000*i + 10*j;
   Y = X;
   for (i=1; i<=5; i++) for (j=i; j<=5; j++) U(i,j) = 100*i + j;
   { X.SubMatrix(3,7,5,9).Inject(U); }
   for (i=1; i<=5; i++) for (j=i; j<=5; j++) Y(i+2,j+4) = 100*i + j;
   Print(Matrix(X-Y));


   // test growing and shrinking a vector
   ColumnVector V(100);
   for (i=1;i<=100;i++) V(i) = i*i+i;
   V = V.Rows(1,50);               // to get first 50 vlaues.

   {
      V.Release(); ColumnVector VX=V;
      V.ReSize(100); V = 0.0; V.Rows(1,50)=VX;
   }                               // V now length 100

   M=V; M=100;                     // to make sure V will hold its values
   for (i=1;i<=50;i++) V(i) -= i*i+i;
   Print(V);

	// test redimensioning vectors with two dimensions given
   ColumnVector CV1(10); CV1 = 10;
   ColumnVector CV2(5); CV2.ReSize(10,1); CV2 = 10;
   V = CV1-CV2; Print(V);

   RowVector RV1(20); RV1 = 100;
   RowVector RV2; RV2.ReSize(1,20); RV2 = 100;
   V = (RV1-RV2).t(); Print(V);

   X.ReSize(4,7);
   for (i=1; i<=4; i++) for (j=1; j<=7; j++) X(i,j) = 1000*i + 10*j;
   Y = 10.5 * X;
   Z = 7.25 - Y;
   M = Z + X * 10.5 - 7.25;
   Print(M);
   Y = 2.5 * X;
   Z = 9.25 + Y;
   M = Z - X * 2.5 - 9.25;
   Print(M);
   U.ReSize(8);
   for (i=1; i<=8; i++) for (j=i; j<=8; j++) U(i,j) = 100*i + j;
   Y = 100 - U;
   M = Y + U - 100;
   Print(M);
   SymmetricMatrix S,T; S << (U + U.t());
   T = 100 - S; M = T + S - 100; Print(M);
   T = 100 - 2 * S; M = T + S * 2 - 100; Print(M);
   X = 100 - 2 * S; M = X + S * 2 - 100; Print(M);
   T = S; T = 100 - T; M = T + S - 100; Print(M);

   // test new
   Matrix* MX; MX = new Matrix; if (!MX) Throw(Bad_alloc("New fails "));
   MX->ReSize(10,20);
   for (i = 1; i <= 10; i++) for (j = 1; j <= 20; j++)
      (*MX)(i,j) = 100 * i + j;
   ColumnVector* CV = new ColumnVector(10);
   if (!CV) Throw(Bad_alloc("New fails "));
   *CV << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << 10;
   RowVector* RV =  new RowVector(CV->t() | (*CV + 10).t());
   if (!RV) Throw(Bad_alloc("New fails "));
   CV1 = ColumnVector(10); CV1 = 1; RV1 = RowVector(20); RV1 = 1;
   *MX -= 100 * *CV * RV1 + CV1 * *RV;
   Print(*MX);
   delete MX; delete CV; delete RV;


//   cout << "\nEnd of second test\n";
}
