//$$newmatrm.cpp                         rectangular matrix operations

// Copyright (C) 1991,2,3,4: R B Davies



#include "newmat.h"
#include "newmatrm.h"

#ifdef use_namespace
namespace NEWMAT {
#endif


// operations on rectangular matrices


void RectMatrixRow::Reset (const Matrix& M, int row, int skip, int length)
{
   RectMatrixRowCol::Reset
      ( M.Store()+row*M.Ncols()+skip, length, 1, M.Ncols() );
}

void RectMatrixRow::Reset (const Matrix& M, int row)
{
   RectMatrixRowCol::Reset( M.Store()+row*M.Ncols(), M.Ncols(), 1, M.Ncols() );
}

void RectMatrixCol::Reset (const Matrix& M, int skip, int col, int length)
{
   RectMatrixRowCol::Reset
      ( M.Store()+col+skip*M.Ncols(), length, M.Ncols(), 1 );
}

void RectMatrixCol::Reset (const Matrix& M, int col)
   { RectMatrixRowCol::Reset( M.Store()+col, M.Nrows(), M.Ncols(), 1 ); }


Real RectMatrixRowCol::SumSquare() const
{
   long_Real sum = 0.0; int i = n; Real* s = store; int d = spacing;
   while (i--) { sum += (long_Real)*s * *s; s += d; }
   return (Real)sum;
}

Real RectMatrixRowCol::operator*(const RectMatrixRowCol& rmrc) const
{
   long_Real sum = 0.0; int i = n;
   Real* s = store; int d = spacing;
   Real* s1 = rmrc.store; int d1 = rmrc.spacing;
   if (i!=rmrc.n)
   {
      Tracer tr("newmatrm");
      Throw(InternalException("Dimensions differ in *"));
   }
   while (i--) { sum += (long_Real)*s * *s1; s += d; s1 += d1; }
   return (Real)sum;
}

void RectMatrixRowCol::AddScaled(const RectMatrixRowCol& rmrc, Real r)
{
   int i = n; Real* s = store; int d = spacing;
   Real* s1 = rmrc.store; int d1 = rmrc.spacing;
   if (i!=rmrc.n)
   {
      Tracer tr("newmatrm");
      Throw(InternalException("Dimensions differ in AddScaled"));
   }
   while (i--) { *s += *s1 * r; s += d; s1 += d1; }
}

void RectMatrixRowCol::Divide(const RectMatrixRowCol& rmrc, Real r)
{
   int i = n; Real* s = store; int d = spacing;
   Real* s1 = rmrc.store; int d1 = rmrc.spacing;
   if (i!=rmrc.n)
   {
      Tracer tr("newmatrm");
      Throw(InternalException("Dimensions differ in Divide"));
   }
   while (i--) { *s = *s1 / r; s += d; s1 += d1; }
}

void RectMatrixRowCol::Divide(Real r)
{
   int i = n; Real* s = store; int d = spacing;
   while (i--) { *s /= r; s += d; }
}

void RectMatrixRowCol::Negate()
{
   int i = n; Real* s = store; int d = spacing;
   while (i--) { *s = - *s; s += d; }
}

void RectMatrixRowCol::Zero()
{
   int i = n; Real* s = store; int d = spacing;
   while (i--) { *s = 0.0; s += d; }
}

void ComplexScale(RectMatrixCol& U, RectMatrixCol& V, Real x, Real y)
{
   int n = U.n;
   if (n != V.n)
   {
      Tracer tr("newmatrm");
      Throw(InternalException("Dimensions differ in ComplexScale"));
   }
   Real* u = U.store; Real* v = V.store; 
   int su = U.spacing; int sv = V.spacing;
   while (n--)
   {
      Real z = *u * x - *v * y;  *v =  *u * y + *v * x;  *u = z;
      u += su;  v += sv;
   }
}

void Rotate(RectMatrixCol& U, RectMatrixCol& V, Real tau, Real s)
{
   //  (U, V) = (U, V) * (c, s)  where  tau = s/(1+c), c^2 + s^2 = 1
   int n = U.n;
   if (n != V.n)
   {
      Tracer tr("newmatrm");
      Throw(InternalException("Dimensions differ in Rotate"));
   }
   Real* u = U.store; Real* v = V.store; 
   int su = U.spacing; int sv = V.spacing;
   while (n--)
   {
      Real zu = *u; Real zv = *v;
      *u -= s * (zv + zu * tau); *v += s * (zu - zv * tau);
      u += su;  v += sv;
   }
}


#ifdef use_namespace
}
#endif


