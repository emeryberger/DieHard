
//#define WANT_STREAM


#include "include.h"

#include "newmatap.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif



// **************************** test program ******************************

void Print(const Matrix& X);
void Print(const UpperTriangularMatrix& X);
void Print(const DiagonalMatrix& X);
void Print(const SymmetricMatrix& X);
void Print(const LowerTriangularMatrix& X);

void Clean(DiagonalMatrix&, Real);

void Transposer(const GenericMatrix& GM1, GenericMatrix&GM2)
   { GM2 = GM1.t(); }

// this is a routine in "Numerical Recipes in C" format
// if R is a row vector, C a column vector and D diagonal
// make matrix DCR

static void DCR(Real d[], Real c[], int m, Real r[], int n, Real **dcr)
{
   int i, j;
   for (i = 1; i <= m; i++) for (j = 1; j <= n; j++)
   dcr[i][j] = d[i] * c[i] * r[j];
}

void trymat8()
{
//   cout << "\nEighth test of Matrix package\n";
   Tracer et("Eighth test of Matrix package");
   Tracer::PrintTrace();

   int i;


   DiagonalMatrix D(6);
   for (i=1;i<=6;i++)  D(i,i)=i*i+i-10;
   DiagonalMatrix D2=D;
   Matrix MD=D;

   DiagonalMatrix D1(6); for (i=1;i<=6;i++) D1(i,i)=-100+i*i*i;
   Matrix MD1=D1;
   Print(Matrix(D*D1-MD*MD1));
   Print(Matrix((-D)*D1+MD*MD1));
   Print(Matrix(D*(-D1)+MD*MD1));
   DiagonalMatrix DX=D;
   {
      Tracer et1("Stage 1");
      DX=(DX+D1)*DX; Print(Matrix(DX-(MD+MD1)*MD));
      DX=D;
      DX=-DX*DX+(DX-(-D1))*((-D1)+DX);
      // Matrix MX = Matrix(MD1);
      // MD1=DX+(MX.t())*(MX.t()); Print(MD1);
      MD1=DX+(Matrix(MD1).t())*(Matrix(MD1).t()); Print(MD1);
      DX=D; DX=DX; DX=D2-DX; Print(DiagonalMatrix(DX));
      DX=D;
   }
   {
      Tracer et1("Stage 2");
      D.Release(2);
      D1=D; D2=D;
      Print(DiagonalMatrix(D1-DX));
      Print(DiagonalMatrix(D2-DX));
      MD1=1.0;
      Print(Matrix(MD1-1.0));
   }
   {
      Tracer et1("Stage 3");
      //GenericMatrix
      LowerTriangularMatrix LT(4);
      LT << 1 << 2 << 3 << 4 << 5 << 6  << 7 << 8 << 9 << 10;
      UpperTriangularMatrix UT = LT.t() * 2.0;
      GenericMatrix GM1 = LT;
      LowerTriangularMatrix LT1 = GM1-LT; Print(LT1);
      GenericMatrix GM2 = GM1; LT1 = GM2; LT1 = LT1-LT; Print(LT1);
      GM2 = GM1; LT1 = GM2; LT1 = LT1-LT; Print(LT1);
      GM2 = GM1*2; LT1 = GM2; LT1 = LT1-LT*2; Print(LT1);
      GM1.Release();
      GM1=GM1; LT1=GM1-LT; Print(LT1); LT1=GM1-LT; Print(LT1);
      GM1.Release();
      GM1=GM1*4; LT1=GM1-LT*4; Print(LT1);
      LT1=GM1-LT*4; Print(LT1); GM1.CleanUp();
      GM1=LT; GM2=UT; GM1=GM1*GM2; Matrix M=GM1; M=M-LT*UT; Print(M);
      Transposer(LT,GM2); LT1 = LT - GM2.t(); Print(LT1);
      GM1=LT; Transposer(GM1,GM2); LT1 = LT - GM2.t(); Print(LT1);
      GM1 = LT; GM1 = GM1 + GM1; LT1 = LT*2-GM1; Print(LT1);
   }
   {
      Tracer et1("Stage 4");
      // Another test of SVD
      Matrix M(12,12); M = 0;
      M(1,1) = M(2,2) = M(4,4) = M(6,6) =
         M(7,7) = M(8,8) = M(10,10) = M(12,12) = -1;
      M(1,6) = M(1,12) = -5.601594;
      M(3,6) = M(3,12) = -0.000165;
      M(7,6) = M(7,12) = -0.008294;
      DiagonalMatrix D;
      SVD(M,D);
      SortDescending(D);
      // answer given by matlab
      DiagonalMatrix DX(12);
      DX(1) = 8.0461;
      DX(2) = DX(3) = DX(4) = DX(5) = DX(6) = DX(7) = 1;
      DX(8) = 0.1243;
      DX(9) = DX(10) = DX(11) = DX(12) = 0;
      D -= DX; Clean(D,0.0001); Print(D);
   }
   {
      Tracer et1("Stage 5");
      // test numerical recipes in C interface
      DiagonalMatrix D(10);
      D << 1 << 4 << 6 << 2 << 1 << 6 << 4 << 7 << 3 << 1;
      ColumnVector C(10);
      C << 3 << 7 << 5 << 1 << 4 << 2 << 3 << 9 << 1 << 3;
      RowVector R(6);
      R << 2 << 3 << 5 << 7 << 11 << 13;
      nricMatrix M(10, 6);
      DCR( D.nric(), C.nric(), 10, R.nric(), 6, M.nric() );
      M -= D * C * R;  Print(M);
   }

//   cout << "\nEnd of eighth test\n";
}
