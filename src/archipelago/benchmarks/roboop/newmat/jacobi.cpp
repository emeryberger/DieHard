//$$jacobi.cpp                           jacobi eigenvalue analysis

// Copyright (C) 1991,2,3,4: R B Davies


//#define WANT_STREAM


#define WANT_MATH

#include "include.h"
#include "newmat.h"
#include "precisio.h"
#include "newmatrm.h"

#ifdef use_namespace
namespace NEWMAT {
#endif


void Jacobi(const SymmetricMatrix& X, DiagonalMatrix& D, SymmetricMatrix& A,
   Matrix& V, bool eivec)
{
   Real epsilon = FloatingPointPrecision::Epsilon();
   Tracer et("Jacobi");
   int n = X.Nrows(); DiagonalMatrix B(n), Z(n); D.ReSize(n); A = X;
   if (eivec) { V.ReSize(n,n); D = 1.0; V = D; }
   B << A; D = B; Z = 0.0; A.Inject(Z);
   for (int i=1; i<=50; i++)
   {
      Real sm=0.0; Real* a = A.Store(); int p = A.Storage();
      while (p--) sm += fabs(*a++);            // have previously zeroed diags
      if (sm==0.0) return;
      Real tresh = (i<4) ? 0.2 * sm / square(n) : 0.0; a = A.Store();
      for (p = 0; p < n; p++)
      {
	 Real* ap1 = a + (p*(p+1))/2;
         Real& zp = Z.element(p); Real& dp = D.element(p);
	 for (int q = p+1; q < n; q++)
	 {
	    Real* ap = ap1; Real* aq = a + (q*(q+1))/2;
            Real& zq = Z.element(q); Real& dq = D.element(q);
            Real& apq = A.element(q,p);
            Real g = 100 * fabs(apq); Real adp = fabs(dp); Real adq = fabs(dq);

	    if (i>4 && g < epsilon*adp && g < epsilon*adq) apq = 0.0;
	    else if (fabs(apq) > tresh)
	    {
	       Real t; Real h = dq - dp; Real ah = fabs(h);
	       if (g < epsilon*ah) t = apq / h;
	       else
	       {
		  Real theta = 0.5 * h / apq;
		  t = 1.0 / ( fabs(theta) + sqrt(1.0 + square(theta)) );
		  if (theta<0.0) t = -t;
	       }
	       Real c = 1.0 / sqrt(1.0 + square(t)); Real s = t * c;
	       Real tau = s / (1.0 + c); h = t * apq;
               zp -= h; zq += h; dp -= h; dq += h; apq = 0.0;
	       int j = p;
	       while (j--)
	       {
		  g = *ap; h = *aq;
		  *ap++ = g-s*(h+g*tau); *aq++ = h+s*(g-h*tau);
	       }
	       int ip = p+1; j = q-ip; ap += ip++; aq++;
	       while (j--)
	       {
		  g = *ap; h = *aq;
                  *ap = g-s*(h+g*tau); *aq++ = h+s*(g-h*tau);
		  ap += ip++;
	       }
	       int iq = q+1; j = n-iq; ap += ip++; aq += iq++;
	       while (j--)
	       {
		  g = *ap; h = *aq;
		  *ap = g-s*(h+g*tau); *aq = h+s*(g-h*tau);
		  ap += ip++; aq += iq++;
	       }
	       if (eivec)
	       {
                  RectMatrixCol VP(V,p); RectMatrixCol VQ(V,q);
                  Rotate(VP, VQ, tau, s);
	       }
	    }
	 }
      }
      B = B + Z; D = B; Z = 0.0;
   }
   Throw(ConvergenceException(X));
}

void Jacobi(const SymmetricMatrix& X, DiagonalMatrix& D)
{ SymmetricMatrix A; Matrix V; Jacobi(X,D,A,V,false); }

void Jacobi(const SymmetricMatrix& X, DiagonalMatrix& D, SymmetricMatrix& A)
{ Matrix V; Jacobi(X,D,A,V,false); }

void Jacobi(const SymmetricMatrix& X, DiagonalMatrix& D, Matrix& V)
{ SymmetricMatrix A; Jacobi(X,D,A,V,true); }


#ifdef use_namespace
}
#endif

