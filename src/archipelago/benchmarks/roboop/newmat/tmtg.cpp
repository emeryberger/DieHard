
//#define WANT_STREAM

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




void trymatg()
{
//   cout << "\nSixteenth test of Matrix package\n";
//   cout << "\n";
   Tracer et("Sixteenth test of Matrix package");
   Tracer::PrintTrace();

   int i,j;
   Matrix M(4,7);
   for (i=1; i<=4; i++) for (j=1; j<=7; j++) M(i,j) = 100 * i + j;
   ColumnVector CV = M.AsColumn();
   {
      Tracer et1("Stage 1");
      RowVector test(7);
      test(1) = SumSquare(M);
      test(2) = SumSquare(CV);
      test(3) = SumSquare(CV.t());
      test(4) = SumSquare(CV.AsDiagonal());
      test(5) = SumSquare(M.AsColumn());
      test(6) = Matrix(CV.t()*CV)(1,1);
      test(7) = (CV.t()*CV).AsScalar();
      test = test - 2156560.0; Print(test);
   }

   UpperTriangularMatrix U(6);
   for (i=1; i<=6; i++) for (j=i; j<=6; j++) U(i,j) = i + (i-j) * (i-j);
   M = U; DiagonalMatrix D; D << U;
   LowerTriangularMatrix L = U.t(); SymmetricMatrix S; S << (L+U)/2.0;
   {
      Tracer et1("Stage 2");
      RowVector test(6);
      test(1) = U.Trace();
      test(2) = L.Trace();
      test(3) = D.Trace();
      test(4) = S.Trace();
      test(5) = M.Trace();
      test(6) = ((L+U)/2.0).Trace();
      test = test - 21; Print(test);
      test.ReSize(5);
      test(1) = LogDeterminant(U).Value();
      test(2) = LogDeterminant(L).Value();
      test(3) = LogDeterminant(D).Value();
      test(4) = LogDeterminant(D).Value();
      test(5) =  LogDeterminant((L+D)/2.0).Value(); // (!=Glockenspiel)
      test = test - 720; Clean(test,0.000000001); Print(test);
   }

   {
      Tracer et1("Stage 3");
      S << L*U; M = S;
      RowVector test(4);
      test(1) = LogDeterminant(S).Value();
      test(2) = LogDeterminant(M).Value();
      test(3) = LogDeterminant(L*U).Value();           // (!=Glockenspiel)
      test(4) = LogDeterminant(Matrix(L*L)).Value();   // (!=Glockenspiel)
      test = test - 720.0 * 720.0; Clean(test,0.00000001); Print(test);
   }

   {
      Tracer et1("Stage 4");
      M = S * S;
      Matrix SX = S;
      RowVector test(3);
      test(1) = SumSquare(S);
      test(2) = SumSquare(SX);
      test(3) = Trace(M);
		test = test - 3925961.0; Print(test);
   }
   {
      Tracer et1("Stage 5");
      SymmetricMatrix SM(10), SN(10);
      Real S = 0.0;
      for (i=1; i<=10; i++) for (j=i; j<=10; j++)
      {
         SM(i,j) = 1.5 * i - j; SN(i,j) = SM(i,j) * SM(i,j);
         if (SM(i,j) < 0.0)   SN(i,j) = - SN(i,j);
         S += SN(i,j) * ((i==j) ? 1.0 : 2.0);
      }
      Matrix M = SM, N = SN; RowVector test(4);
      test(1) = SumAbsoluteValue(SN);
      test(2) = SumAbsoluteValue(-SN);
      test(3) = SumAbsoluteValue(N);
      test(4) = SumAbsoluteValue(-N);
      test = test - 1168.75; Print(test);
      test(1) = Sum(SN);
      test(2) = -Sum(-SN);
      test(3) = Sum(N);
      test(4) = -Sum(-N);
      test = test - S; Print(test);
      test(1) = MaximumAbsoluteValue(SM);
      test(2) = MaximumAbsoluteValue(-SM);
      test(3) = MaximumAbsoluteValue(M);
      test(4) = MaximumAbsoluteValue(-M);
      test = test - 8.5; Print(test);
   }
   {
      Tracer et1("Stage 6");
      Matrix M(15,20); Real value = 0.0;
      for (i=1; i<=15; i++) { for (j=1; j<=20; j++) M(i,j) = 1.5 * i - j; }
      for (i=1; i<=20; i++)
      { Real v = SumAbsoluteValue(M.Column(i)); if (value<v) value = v; }
      RowVector test(3);
      test(1) = value;
      test(2) = Norm1(M);
      test(3) = NormInfinity(M.t());
      test = test - 165; Print(test);
      test.ReSize(5);
      ColumnVector CV = M.AsColumn(); M = CV.t();
      test(1) = Norm1(CV.t());
      test(2) = MaximumAbsoluteValue(M);
      test(3) = NormInfinity(CV);
      test(4) = Norm1(M);
      test(5) = NormInfinity(M.t());
      test = test - 21.5; Print(test);
   }   

//   cout << "\nEnd of Sixteenth test\n";
}
