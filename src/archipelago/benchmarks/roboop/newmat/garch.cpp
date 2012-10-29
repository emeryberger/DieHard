
#define WANT_STREAM
#define WANT_MATH

#include "newmatap.h"
#include "newmatio.h"
#include "newmatnl.h"
#include "fstream.h"

#ifdef use_namespace
using namespace RBD_LIBRARIES;
#endif

inline Real square(Real x) { return x*x; }

// the class that defines the GARCH log-likelihood

class GARCH11_LL : public LL_D_FI
{
   ColumnVector Y;                 // Y values
   ColumnVector X;                 // X values
   ColumnVector D;                 // derivatives of loglikelihood
   SymmetricMatrix D2;             // - approximate second derivatives
   int n;                          // number of observations
   Real beta, alpha0, alpha1, beta1;
                                   // the parameters

public:

   GARCH11_LL(const ColumnVector& y, const ColumnVector& x)
      : Y(y), X(x), n(y.Nrows()) {}
                                   // constructor - load Y and X values

   void Set(const ColumnVector& p) // set parameter values
   {
      para = p; beta = para(1); alpha0 = para(2);
      alpha1 = para(3); beta1 = para(4);
   }

   bool IsValid();                 // are parameters valid
   Real LogLikelihood();           // return the loglikelihood
   ReturnMatrix Derivatives();     // derivatives of log-likelihood
   ReturnMatrix FI();              // Fisher Information matrix
};

bool GARCH11_LL::IsValid()
{ return alpha0>0 && alpha1>0 && beta1>0 && (alpha1+beta1)<1.0; }

Real GARCH11_LL::LogLikelihood()
{
//   cout << endl << "                           ";
//   cout << setw(10) << setprecision(5) << beta;
//   cout << setw(10) << setprecision(5) << alpha0;
//   cout << setw(10) << setprecision(5) << alpha1;
//   cout << setw(10) << setprecision(5) << beta1;
   ColumnVector H(n);
   Real denom = (1-alpha1-beta1);
   H(1) = alpha0/denom;    // the expected value of H
   ColumnVector U = Y - X * beta;
   ColumnVector LH(n);     // derivative of log-likelihood wrt H
			   // each row corresponds to one observation
   LH(1)=0;
   Matrix Hderiv(n,4);     // rectangular matrix of derivatives
			   // of H wrt parameters
			   // each row corresponds to one observation
			   // each column to one of the parameters
   Hderiv(1,1) = 0;
   Hderiv(1,2) = 1.0/denom;
   Hderiv(1,3) = alpha0 / square(denom);
   Hderiv(1,4) = Hderiv(1,3);
   Real LL = 0.0;          // the log likelihood
   Real sum1 = 0;          // for forming derivative wrt beta
   Real sum2 = 0;          // for forming second derivative wrt beta
   for (int i=2; i<=n; i++)
   {
      Real u1 = U(i-1); Real h1 = H(i-1);
      Real h = alpha0 + alpha1*square(u1) + beta1*h1;
      H(i) = h; Real u = U(i);
      LL += log(h) + square(u) / h;
      Hderiv(i,1) = -2*u1*alpha1*X(i-1) + beta1*Hderiv(i-1,1);
      Hderiv(i,2) = 1 + beta1*Hderiv(i-1,2);
      Hderiv(i,3) = square(u1) + beta1*Hderiv(i-1,3);
      Hderiv(i,4) = h1 + beta1*Hderiv(i-1,4);
      LH(i) = -0.5 * (1/h - square(u/h));
      sum1 += u * X(i)/ h;
      sum2 += square(X(i)) / h;
   }
   D = Hderiv.t()*LH;
   D(1) += sum1;
   if (wg)                    // do only if second derivatives wanted
   {
      Hderiv.Row(1) = 0.0;
      Hderiv = H.AsDiagonal().i() * Hderiv;
      D2 << Hderiv.t() * Hderiv;  D2 = D2 / 2.0;
      D2(1,1) += sum2;
   }
   return -0.5 * LL;
}

ReturnMatrix GARCH11_LL::Derivatives()
{ return D; }

ReturnMatrix GARCH11_LL::FI()
{
   if (!wg) cout << endl << "unexpected call of FI" << endl;
   return D2;
}



main()
{
   // get data
   ifstream fin("garch.dat");
   if (!fin) { cout << "can't find garch.dat\n"; exit(1); }
   int n; fin >> n;            // series length
   // Y contains the dependant variable, X the predictor variable
   ColumnVector Y(n), X(n);
   int i;
   for (i=1; i<=n; i++) fin >> Y(i) >> X(i);
   cout << "Read " << n << " data points - begin fit\n\n";
   // now do the fit
   ColumnVector H(n);
   GARCH11_LL garch11(Y,X);                  // loglikehood "object"
   MLE_D_FI mle_d_fi(garch11,100,0.0001);    // mle "object"
   ColumnVector Para(4);                     // to hold the parameters
   Para << 0.0 << 0.1 << 0.1 << 0.1;         // starting values
      // (Should change starting values to a more intelligent formula)
   mle_d_fi.Fit(Para);                       // do the fit
   ColumnVector SE;
   mle_d_fi.GetStandardErrors(SE);
   cout << "\n\n";
   cout << "estimates and standard errors\n";
   cout << setw(15) << setprecision(5) << (Para | SE) << endl << endl;
   SymmetricMatrix Corr;
   mle_d_fi.GetCorrelations(Corr);
   cout << "correlation matrix\n";
   cout << setw(10) << setprecision(2) << Corr << endl << endl;
   cout << "inverse of correlation matrix\n";
   cout << setw(10) << setprecision(2) << Corr.i() << endl << endl;
   return 0;
}



