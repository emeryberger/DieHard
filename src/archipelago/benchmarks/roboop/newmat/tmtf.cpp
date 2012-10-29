
//#define WANT_STREAM
#define WANT_MATH

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


static void SlowFT(const ColumnVector& a, const ColumnVector&b,
   ColumnVector& x, ColumnVector& y)
{
   int n = a.Nrows();
   x.ReSize(n); y.ReSize(n);
   Real f = 6.2831853071795864769/n;
   for (int j=1; j<=n; j++)
   {
      Real sumx = 0.0; Real sumy = 0.0;
      for (int k=1; k<=n; k++)
      {
	 Real theta = - (j-1) * (k-1) * f;
	 Real c = cos(theta); Real s = sin(theta);
	 sumx += c * a(k) - s * b(k); sumy += s * a(k) + c * b(k);
      }
      x(j) = sumx; y(j) = sumy;
   }
}

static void SlowDTT_II(const ColumnVector& a, ColumnVector& c, ColumnVector& s)
{
   int n = a.Nrows(); c.ReSize(n); s.ReSize(n);
   Real f = 6.2831853071795864769 / (4*n);
   int k;

   for (k=1; k<=n; k++)
   {
      Real sum = 0.0;
      const int k1 = k-1;              // otherwise Visual C++ 5 fails
      for (int j=1; j<=n; j++) sum += cos(k1 * (2*j-1) * f) * a(j);
      c(k) = sum;
   }

   for (k=1; k<=n; k++)
   {
      Real sum = 0.0;
      for (int j=1; j<=n; j++) sum += sin(k * (2*j-1) * f) * a(j);
      s(k) = sum;
   }
}

static void SlowDTT(const ColumnVector& a, ColumnVector& c, ColumnVector& s)
{
   int n1 = a.Nrows(); int n = n1 - 1;
   c.ReSize(n1); s.ReSize(n1);
   Real f = 6.2831853071795864769 / (2*n);
   int k;

   int sign = 1;
   for (k=1; k<=n1; k++)
   {
      Real sum = 0.0;
      for (int j=2; j<=n; j++) sum += cos((j-1) * (k-1) * f) * a(j);
      c(k) = sum + (a(1) + sign * a(n1)) / 2.0;
      sign = -sign;
   }

   for (k=2; k<=n; k++)
   {
      Real sum = 0.0;
      for (int j=2; j<=n; j++) sum += sin((j-1) * (k-1) * f) * a(j);
      s(k) = sum;
   }
   s(1) = s(n1) = 0;
}

static void test(int n)
{
   Tracer et("Test FFT");

   ColumnVector A(n), B(n), X, Y;
   for (int i=0; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A.element(i) = fabs(sin(7.0*f) + 0.5 * cos(19.0 * f)) + (Real)i/(Real)n;
      B.element(i) = fabs(0.25 * cos(31.0 * f)) + (Real)i/(Real)n;
   }
   FFT(A, B, X, Y); FFTI(X, Y, X, Y);
   X = X - A; Y = Y - B;
   Clean(X,0.000000001); Clean(Y,0.000000001); Print(X); Print(Y);
}

static void test1(int n)
{
   Tracer et("Test RealFFT");

   ColumnVector A(n), B(n), X, Y;
   for (int i=0; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A.element(i) = fabs(sin(7.0*f) + 0.5 * cos(19.0 * f)) + (Real)i/(Real)n;
   }
   B = 0.0;
   FFT(A, B, X, Y);
   B.CleanUp();                 // release some space
   int n2 = n/2+1;
   ColumnVector X1,Y1,X2,Y2;
   RealFFT(A, X1, Y1);
   X2 = X1 - X.Rows(1,n2); Y2 = Y1 - Y.Rows(1,n2);
   Clean(X2,0.000000001); Clean(Y2,0.000000001); Print(X2); Print(Y2);
   X2.CleanUp(); Y2.CleanUp();  // release some more space
   RealFFTI(X1,Y1,B);
   B = A - B;
   Clean(B,0.000000001); Print(B);
}

static void test2(int n)
{
   Tracer et("cf FFT and slow FT");

   ColumnVector A(n), B(n), X, Y, X1, Y1;
   for (int i=0; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A.element(i) = fabs(sin(7.0*f) - 0.5 * cos(19.0 * f)) + (Real)i/(Real)n;
      B.element(i) = fabs(0.25 * cos(31.0 * f)) - (Real)i/(Real)n;
   }
   FFT(A, B, X, Y);
   SlowFT(A, B, X1, Y1);
   X = X - X1; Y = Y - Y1;
   Clean(X,0.000000001); Clean(Y,0.000000001); Print(X); Print(Y);
}

static void test3(int n)
{
   Tracer et("cf slow and fast DCT_II and DST_II");

   ColumnVector A(n), X, Y, X1, Y1;
   for (int i=0; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A.element(i) = fabs(sin(7.0*f) - 0.55 * cos(19.0 * f)
		  + .73 * sin(6.0 * f)) + (Real)i/(Real)n;
   }
   DCT_II(A, X); DST_II(A, Y);
   SlowDTT_II(A, X1, Y1);
   X -= X1; Y -= Y1;
   Clean(X,0.000000001); Clean(Y,0.000000001); Print(X); Print(Y);
}

static void test4(int n)
{
   Tracer et("Test DCT_II");

   ColumnVector A1(n);
   for (int i=0; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A1.element(i) =
         fabs(sin(7.0*f) + 0.7588 * cos(19.0 * f) + (Real)i/(Real)n);
   }
   // do DCT II by ordinary FFT
   ColumnVector P(2*n), Q(2*n);
   P = 0.0; Q = 0.0; P.Rows(1,n) = A1;
   FFT(P, Q, P, Q);
   ColumnVector B1(n);
   for (int k=0; k<n; k++)
   {
      Real arg = k * 6.2831853071795864769 / (4 * n);
      B1(k+1) = P(k+1) * cos(arg) + Q(k+1) * sin(arg);
   }
   // use DCT_II routine
   ColumnVector B2;
   DCT_II(A1,B2);
   // test inverse
   ColumnVector A2;
   DCT_II_inverse(B2,A2);
   A1 -= A2; B1 -= B2;
   Clean(A1,0.000000001); Clean(B1,0.000000001); Print(A1); Print(B1);
}

static void test5(int n)
{
   Tracer et("Test DST_II");

   ColumnVector A1(n);
   for (int i=0; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A1.element(i) =
         fabs(sin(11.0*f) + 0.7588 * cos(19.0 * f) + (Real)i/(Real)n);
   }
   // do DST II by ordinary FFT
   ColumnVector P(2*n), Q(2*n);
   P = 0.0; Q = 0.0; P.Rows(1,n) = A1;
   FFT(P, Q, P, Q);
   ColumnVector B1(n);
   for (int k=1; k<=n; k++)
   {
      Real arg = k * 6.2831853071795864769 / (4 * n);
      B1(k) = P(k+1) * sin(arg) - Q(k+1) * cos(arg);
   }
   // use DST_II routine
   ColumnVector B2;
   DST_II(A1,B2);
   // test inverse
   ColumnVector A2;
   DST_II_inverse(B2,A2);
   A1 -= A2; B1 -= B2;
   Clean(A1,0.000000001); Clean(B1,0.000000001); Print(A1); Print(B1);
}

static void test6(int n)
{
   Tracer et("Test DST");

   ColumnVector A1(n+1);
   A1(1) = A1(n+1) = 0;
   for (int i=1; i<n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A1.element(i) =
         fabs(sin(11.0*f) + 0.7588 * cos(19.0 * f) + (Real)i/(Real)n);
   }
   // do DST by ordinary FFT
   ColumnVector P(2*n), Q(2*n); P = 0.0; Q = 0.0; P.Rows(1,n+1) = A1;
   FFT(P, Q, P, Q);
   ColumnVector B1 = -Q.Rows(1,n+1);
   // use DST routine
   ColumnVector B2;
   DST(A1,B2);
   // test inverse
   ColumnVector A2;
   DST_inverse(B2,A2);
   A1 -= A2; B1 -= B2;
   Clean(A1,0.000000001); Clean(B1,0.000000001); Print(A1); Print(B1);
}



static void test7(int n)
{
   Tracer et("Test DCT");

   ColumnVector A1(n+1);
   for (int i=0; i<=n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A1.element(i) =
         fabs(sin(17.0*f) + 0.6399 * cos(23.0 * f) + 1.32*(Real)i/(Real)n);
   }
   // do DCT by ordinary FFT
   ColumnVector P(2*n), Q(2*n); P = 0.0; Q = 0.0; P.Rows(1,n+1) = A1;
   P(1) /= 2.0; P(n+1) /= 2.0;
   FFT(P, Q, P, Q);
   ColumnVector B1 = P.Rows(1,n+1);
   // use DCT routine
   ColumnVector B2;
   DCT(A1,B2);
   // test inverse
   ColumnVector A2;
   DCT_inverse(B2,A2);
   A1 -= A2; B1 -= B2;
   Clean(A1,0.000000001); Clean(B1,0.000000001); Print(A1); Print(B1);
}

static void test8(int n)
{
   Tracer et("cf slow and fast DCT and DST");

   ColumnVector A(n+1), X, Y, X1, Y1;
   for (int i=0; i<=n; i++)
   {
      Real f = 6.2831853071795864769*i/n;
      A.element(i) = fabs(sin(7.0*f) - 0.5 * cos(19.0 * f) +
         0.3 * (Real)i/(Real)n);
   }
   DCT(A, X); DST(A, Y);
   SlowDTT(A, X1, Y1);
   X -= X1; Y -= Y1;
   Clean(X,0.000000001); Clean(Y,0.000000001); Print(X); Print(Y);
}




void trymatf()
{
   Tracer et("Fifteenth test of Matrix package");
   Tracer::PrintTrace();

   ColumnVector A(12), B(12);
   for (int i = 1; i <=12; i++)
   {
      Real i1 = i - 1;
      A(i) = .7
	   + .2 * cos(6.2831853071795864769 * 4.0 * i1 / 12)
	   + .3 * sin(6.2831853071795864769 * 3.0 * i1 / 12);
      B(i) = .9
	   + .5 * sin(6.2831853071795864769 * 2.0 * i1 / 12)
	   + .4 * cos(6.2831853071795864769 * 1.0 * i1 / 12);
   }
   FFT(A, B, A, B);
   A(1) -= 8.4; A(3) -= 3.0; A(5) -= 1.2; A(9) -= 1.2; A(11) += 3.0;
   B(1) -= 10.8; B(2) -= 2.4; B(4) += 1.8; B(10) -= 1.8; B(12) -= 2.4;
   Clean(A,0.000000001); Clean(B,0.000000001); Print(A); Print(B);

   
   // test FFT
   test(2048); test(2000); test(27*81); test(2310); test(49*49);
   test(13*13*13); test(43*47);

   // test realFFT
   test1(2); test1(98); test1(22); test1(100);
   test1(2048); test1(2000); test1(35*35*2);

   // compare FFT and slowFFT
   test2(1); test2(13); test2(12); test2(9); test2(16); test2(30); test2(42);

   // compare DCT_II, DST_II and slow versions
   test3(2); test3(26); test3(32); test3(18);

   // test DCT_II and DST_II
   test4(2); test5(2);
   test4(202); test5(202);
   test4(1000); test5(1000);

   // test DST and DCT
   test6(2); test7(2);
   test6(274); test7(274);
   test6(1000); test7(1000);

   // compare DCT, DST and slow versions
   test8(2); test8(26); test8(32); test8(18);

}
