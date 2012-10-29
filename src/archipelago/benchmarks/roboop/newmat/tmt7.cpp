
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

void trymat7()
{
//   cout << "\nSeventh test of Matrix package\n";
   Tracer et("Seventh test of Matrix package");
   Tracer::PrintTrace();

   int i,j;


   DiagonalMatrix D(6);
   UpperTriangularMatrix U(6);
   for (i=1;i<=6;i++) { for (j=i;j<=6;j++) U(i,j)=i*i*j-50; D(i,i)=i*i+i-10; }
   LowerTriangularMatrix L=(U*3.0).t();
   SymmetricMatrix S(6);
   for (i=1;i<=6;i++) for (j=i;j<=6;j++) S(i,j)=i*i+2.0+j;
   Matrix MD=D; Matrix ML=L; Matrix MU=U;
   Matrix MS=S;
   Matrix M(6,6);
   for (i=1;i<=6;i++) for (j=1;j<=6;j++) M(i,j)=i*j+i*i-10.0;  
   {
      Tracer et1("Stage 1");
      Print(Matrix((S-M)-(MS-M)));
      Print(Matrix((-M-S)+(MS+M)));
      Print(Matrix((U-M)-(MU-M)));
   }
   {
      Tracer et1("Stage 2");
      Print(Matrix((L-M)+(M-ML)));
      Print(Matrix((D-M)+(M-MD)));
      Print(Matrix((D-S)+(MS-MD)));
      Print(Matrix((D-L)+(ML-MD)));
   }

   { M=MU.t(); }
   LowerTriangularMatrix LY=D.i()*U.t();
   {
      Tracer et1("Stage 3");
      MS=D*LY-M; Clean(MS,0.00000001); Print(MS);
      L=U.t();
      LY=D.i()*L; MS=D*LY-M; Clean(MS,0.00000001); Print(MS);
   }
   {
      Tracer et1("Stage 4");
      UpperTriangularMatrix UT(11);
      int i, j;
      for (i=1;i<=11;i++) for (j=i;j<=11;j++) UT(i,j)=i*i+j*3;
      GenericMatrix GM; Matrix X;
      UpperBandMatrix UB(11,3); UB.Inject(UT); UT = UB;
      UpperBandMatrix UB2 = UB / 8;
      GM = UB2-UT/8; X = GM; Print(X);
      SymmetricBandMatrix SB(11,4); SB << (UB + UB.t());
      X = SB - UT - UT.t(); Print(X);
      BandMatrix B = UB + UB.t()*2;
      DiagonalMatrix D; D << B;
      X.ReSize(1,1); X(1,1) = Trace(B)-Sum(D); Print(X);
      X = SB + 5; Matrix X1=X; X = SP(UB,X); Matrix X2 =UB;
      X1 = (X1.AsDiagonal() * X2.AsDiagonal()).AsRow()-X.AsColumn().t();
      Print(X1);
      X1=SB.t(); X2 = B.t(); X = SB.i() * B - X1.i() * X2.t();
      Clean(X,0.00000001); Print(X);
      X = SB.i(); X = X * B - X1.i() * X2.t();
      Clean(X,0.00000001); Print(X);
      D = 1; X = SB.i() * SB - D; Clean(X,0.00000001); Print(X);
      ColumnVector CV(11);
      CV << 2 << 6 <<3 << 8 << -4 << 17.5 << 2 << 1 << -2 << 5 << 3.75;
      D << 2 << 6 <<3 << 8 << -4 << 17.5 << 2 << 1 << -2 << 5 << 3.75;
      X = CV.AsDiagonal(); X = X-D; Print(X);
      SymmetricBandMatrix SB1(11,7); SB1 = 5; 
      SymmetricBandMatrix SB2 = SB1 + D;
      X.ReSize(11,11); X=0;
      for (i=1;i<=11;i++) for (j=1;j<=11;j++)
      {
         if (abs(i-j)<=7) X(i,j)=5;
         if (i==j) X(i,j)+=CV(i);
      }
      SymmetricMatrix SM; SM.ReSize(11);
      SM=SB; SB = SB+SB2; X1 = SM+X-SB; Print(X1);
      SB2=0; X2=SB2; X1=SB; Print(X2);
      for (i=1;i<=11;i++) SB2.Column(i)<<SB.Column(i);
      X1=X1-SB2; Print(X1);
      X = SB; SB2.ReSize(11,4); SB2 = SB*5; SB2 = SB + SB2;
      X1 = X*6 - SB2; Print(X1);
      X1 = SP(SB,SB2/3); X1=X1-SP(X,X*2); Print(X1);
      X1 = SP(SB2/6,X*2); X1=X1-SP(X*2,X); Print(X1);
   }


//   cout << "\nEnd of seventh test\n";
}
