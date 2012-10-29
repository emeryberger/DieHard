
//#define WANT_STREAM


#include "include.h"
#include "newmat.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif


void Print(const Matrix& X);
void Print(const UpperTriangularMatrix& X);
void Print(const DiagonalMatrix& X);
void Print(const SymmetricMatrix& X);
void Print(const LowerTriangularMatrix& X);

void Clean(Matrix&, Real);



void trymatc()
{
//   cout << "\nTwelfth test of Matrix package\n";
   Tracer et("Twelfth test of Matrix package");
   Tracer::PrintTrace();
   DiagonalMatrix D(15); D=1.5;
   Matrix A(15,15);
   int i,j;
   for (i=1;i<=15;i++) for (j=1;j<=15;j++) A(i,j)=i*i+j-150;
   { A = A + D; }
   ColumnVector B(15);
   for (i=1;i<=15;i++) B(i)=i+i*i-150.0;
   {
      Tracer et1("Stage 1");
      ColumnVector B1=B;
      B=(A*2.0).i() * B1;
      Matrix X = A*B-B1/2.0;
      Clean(X, 0.000000001); Print(X);
      A.ReSize(3,5);
      for (i=1; i<=3; i++) for (j=1; j<=5; j++) A(i,j) = i+100*j;

      B = A.AsColumn()+10000;
      RowVector R = (A+10000).AsColumn().t();
      Print( RowVector(R-B.t()) );
   }

   {
      Tracer et1("Stage 2");
      B = A.AsColumn()+10000;
      Matrix XR = (A+10000).AsMatrix(15,1).t();
      Print( RowVector(XR-B.t()) );
   }

   {
      Tracer et1("Stage 3");
      B = (A.AsMatrix(15,1)+A.AsColumn())/2.0+10000;
      Matrix MR = (A+10000).AsColumn().t();
      Print( RowVector(MR-B.t()) );

      B = (A.AsMatrix(15,1)+A.AsColumn())/2.0;
      MR = A.AsColumn().t();
      Print( RowVector(MR-B.t()) );
   }

   {
      Tracer et1("Stage 4");
      B = (A.AsMatrix(15,1)+A.AsColumn())/2.0;
      RowVector R = A.AsColumn().t();
      Print( RowVector(R-B.t()) );
   }

   {
      Tracer et1("Stage 5");
      RowVector R = (A.AsColumn()-5000).t();
      B = ((R.t()+10000) - A.AsColumn())-5000;
      Print( RowVector(B.t()) );
   }

   {
      Tracer et1("Stage 6");
      B = A.AsColumn(); ColumnVector B1 = (A+10000).AsColumn() - 10000;
      Print(ColumnVector(B1-B));
   }

   {
      Tracer et1("Stage 7");
      Matrix X = B.AsMatrix(3,5); Print(Matrix(X-A));
      for (i=1; i<=3; i++) for (j=1; j<=5; j++) B(5*(i-1)+j) -= i+100*j;
      Print(B);
   }

   {
      Tracer et1("Stage 8");
      A.ReSize(7,7); D.ReSize(7);
      for (i=1; i<=7; i++) for (j=1; j<=7; j++) A(i,j) = i*j*j;
      for (i=1; i<=7; i++) D(i,i) = i;
      UpperTriangularMatrix U; U << A;
      Matrix X = A; for (i=1; i<=7; i++) X(i,i) = i;
      A.Inject(D); Print(Matrix(X-A));
      X = U; U.Inject(D); A = U; for (i=1; i<=7; i++) X(i,i) = i;
      Print(Matrix(X-A));
   }

   {
      Tracer et1("Stage 9");
      A.ReSize(7,5);
      for (i=1; i<=7; i++) for (j=1; j<=5; j++) A(i,j) = i+100*j;
      Matrix Y = A; Y = Y - ((const Matrix&)A); Print(Y);
      Matrix X = A; // X.Release();
      Y = A; Y = ((const Matrix&)X) - A; Print(Y); Y = 0.0;
      Y = ((const Matrix&)X) - ((const Matrix&)A); Print(Y);
   }

   {
      Tracer et1("Stage 10");
      // some tests on submatrices
      UpperTriangularMatrix U(20);
      for (i=1; i<=20; i++) for (j=i; j<=20; j++) U(i,j)=100 * i + j;
      UpperTriangularMatrix V = U.SymSubMatrix(1,5);
      UpperTriangularMatrix U1 = U;
      U1.SubMatrix(4,8,5,9) /= 2;
      U1.SubMatrix(4,8,5,9) += 388 * V;
      U1.SubMatrix(4,8,5,9) *= 2;
      U1.SubMatrix(4,8,5,9) += V;
      U1 -= U; UpperTriangularMatrix U2 = U1;
      U1 << U1.SubMatrix(4,8,5,9);
      U2.SubMatrix(4,8,5,9) -= U1; Print(U2);
      U1 -= (777*V); Print(U1);

      U1 = U; U1.SubMatrix(4,8,5,9) -= U.SymSubMatrix(1,5);
      U1 -= U;  U2 = U1; U1 << U1.SubMatrix(4,8,5,9);
      U2.SubMatrix(4,8,5,9) -= U1; Print(U2);
      U1 += V; Print(U1);

      U1 = U;
      U1.SubMatrix(3,10,15,19) += 29;
      U1 -= U;
      Matrix X = U1.SubMatrix(3,10,15,19); X -= 29; Print(X);
      U1.SubMatrix(3,10,15,19) *= 0; Print(U1);

      LowerTriangularMatrix L = U.t();
      LowerTriangularMatrix M = L.SymSubMatrix(1,5);
      LowerTriangularMatrix L1 = L;
      L1.SubMatrix(5,9,4,8) /= 2;
      L1.SubMatrix(5,9,4,8) += 388 * M;
      L1.SubMatrix(5,9,4,8) *= 2;
      L1.SubMatrix(5,9,4,8) += M;
      L1 -= L; LowerTriangularMatrix L2 = L1;
      L1 << L1.SubMatrix(5,9,4,8);
      L2.SubMatrix(5,9,4,8) -= L1; Print(L2);
      L1 -= (777*M); Print(L1);

      L1 = L; L1.SubMatrix(5,9,4,8) -= L.SymSubMatrix(1,5);
      L1 -= L; L2 =L1; L1 << L1.SubMatrix(5,9,4,8);
      L2.SubMatrix(5,9,4,8) -= L1; Print(L2);
      L1 += M; Print(L1);

      L1 = L;
      L1.SubMatrix(15,19,3,10) -= 29;
      L1 -= L;
      X = L1.SubMatrix(15,19,3,10); X += 29; Print(X);
      L1.SubMatrix(15,19,3,10) *= 0; Print(L1);
   }

   {
      Tracer et1("Stage 11");
      // more tests on submatrices
      Matrix M(20,30);
      for (i=1; i<=20; i++) for (j=1; j<=30; j++) M(i,j)=100 * i + j;
      Matrix M1 = M;

      for (j=1; j<=30; j++)
         { ColumnVector CV = 3 * M1.Column(j); M.Column(j) += CV; }
      for (i=1; i<=20; i++)
         { RowVector RV = 5 * M1.Row(i); M.Row(i) -= RV; }

      M += M1; Print(M);
 
   }


//   cout << "\nEnd of twelfth test\n";
}
