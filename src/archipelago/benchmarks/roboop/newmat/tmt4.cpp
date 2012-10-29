
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
void Clean(Matrix&, Real);

void trymat4()
{
//   cout << "\nFourth test of Matrix package\n";
   Tracer et("Fourth test of Matrix package");
   Tracer::PrintTrace();

   int i,j;

   {
      Tracer et1("Stage 1");
      Matrix M(10,10);
      UpperTriangularMatrix U(10);
      for (i=1;i<=10;i++) for (j=1;j<=10;j++) M(i,j) = 100*i+j;
      U << -M;
      Matrix X1 = M.Rows(2,4);
      Matrix Y1 = U.t().Rows(2,4);
      Matrix X = U; { Print(Matrix(X.Columns(2,4).t()-Y1)); }
      RowVector RV = M.Row(5);
      {
         X.ReSize(3,10);
         X.Row(1) << M.Row(2); X.Row(2) << M.Row(3); X.Row(3) << M.Row(4);
         Print(Matrix(X-X1));
      }
      {
         UpperTriangularMatrix V = U.SymSubMatrix(3,5);
         Matrix MV = U.SubMatrix(3,5,3,5); { Print(Matrix(MV-V)); }
         Matrix X2 = M.t().Columns(2,4); { Print(Matrix(X2-X1.t())); }
         Matrix Y2 = U.Columns(2,4); { Print(Matrix(Y2-Y1.t())); }
         ColumnVector CV = M.t().Column(5); { Print(ColumnVector(CV-RV.t())); }
         X.ReSize(10,3); M = M.t();
         X.Column(1) << M.Column(2); X.Column(2) << M.Column(3);
         X.Column(3) << M.Column(4);
         Print(Matrix(X-X2));
      }
   }

   {
      Tracer et1("Stage 2");
      Matrix M; Matrix X; M.ReSize(5,8);
      for (i=1;i<=5;i++) for (j=1;j<=8;j++) M(i,j) = 100*i+j;
      {
         X = M.Columns(5,8); M.Columns(5,8) << M.Columns(1,4);
             M.Columns(1,4) << X;
         X = M.Columns(3,4); M.Columns(3,4) << M.Columns(1,2);
             M.Columns(1,2) << X;
         X = M.Columns(7,8); M.Columns(7,8) << M.Columns(5,6);
             M.Columns(5,6) << X;
      }
      {
         X = M.Column(2); M.Column(2) = M.Column(1); M.Column(1) = X;
         X = M.Column(4); M.Column(4) = M.Column(3); M.Column(3) = X;
         X = M.Column(6); M.Column(6) = M.Column(5); M.Column(5) = X;
         X = M.Column(8); M.Column(8) = M.Column(7); M.Column(7) = X;
         X.ReSize(5,8);
      }
      for (i=1;i<=5;i++) for (j=1;j<=8;j++) X(i,9-j) = 100*i+j;
      Print(Matrix(X-M));
   }
   {
      Tracer et1("Stage 3");
      // try submatrices of zero dimension
      Matrix A(4,5); Matrix B, C;
      for (i=1; i<=4; i++) for (j=1; j<=5; j++)
         A(i,j) = 100+i*10+j;
      B = A + 100;
      C = A | B.Columns(4,3); Print(Matrix(A - C));
      C = A | B.Columns(1,0); Print(Matrix(A - C));
      C = A | B.Columns(6,5); Print(Matrix(A - C));
      C = A & B.Rows(2,1); Print(Matrix(A - C));
   }
   {
      Tracer et1("Stage 4");
      BandMatrix BM(5,3,2);
      BM(1,1) = 1; BM(1,2) = 2; BM(1,3) = 3;
      BM(2,1) = 4; BM(2,2) = 5; BM(2,3) = 6; BM(2,4) = 7;
      BM(3,1) = 8; BM(3,2) = 9; BM(3,3) =10; BM(3,4) =11; BM(3,5) =12;
      BM(4,1) =13; BM(4,2) =14; BM(4,3) =15; BM(4,4) =16; BM(4,5) =17;
                   BM(5,2) =18; BM(5,3) =19; BM(5,4) =20; BM(5,5) =21;
      SymmetricBandMatrix SM(5,3);
      SM.Inject(BandMatrix(BM + BM.t()));
      Matrix A = BM + 1;
      Matrix M = A + A.t() - 2;
      Matrix C = A.i() * BM;
      C = A * C - BM; Clean(C, 0.000000001); Print(C);
      C = A.i() * SM;
      C = A * C - M; Clean(C, 0.000000001); Print(C);
   }
   {
      Tracer et1("Stage 5");
      Matrix X(4,4);
      X << 1 << 2 << 3 << 4
        << 5 << 6 << 7 << 8
        << 9 <<10 <<11 <<12
        <<13 <<14 <<15 <<16;
      Matrix Y(4,0);
      Y = X | Y;
      X -= Y; Print(X);

   }
//   cout << "\nEnd of fourth test\n";
}

