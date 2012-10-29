//$$ newmatap.h           definition file for matrix package applications

// Copyright (C) 1991,2,3,4: R B Davies

#ifndef NEWMATAP_LIB
#define NEWMATAP_LIB 0

#include "newmat.h"

#ifdef use_namespace
namespace NEWMAT {
#endif


/**************************** applications *****************************/


void QRZT(Matrix&, LowerTriangularMatrix&);

void QRZT(const Matrix&, Matrix&, Matrix&);

void QRZ(Matrix&, UpperTriangularMatrix&);

void QRZ(const Matrix&, Matrix&, Matrix&);

inline void HHDecompose(Matrix& X, LowerTriangularMatrix& L)
{ QRZT(X,L); }

inline void HHDecompose(const Matrix& X, Matrix& Y, Matrix& M)
{ QRZT(X, Y, M); }

ReturnMatrix Cholesky(const SymmetricMatrix&);

ReturnMatrix Cholesky(const SymmetricBandMatrix&);

void SVD(const Matrix&, DiagonalMatrix&, Matrix&, Matrix&,
    bool=true, bool=true);


void SVD(const Matrix&, DiagonalMatrix&);

inline void SVD(const Matrix& A, DiagonalMatrix& D, Matrix& U,
   bool withU = true) { SVD(A, D, U, U, withU, false); }

void Jacobi(const SymmetricMatrix&, DiagonalMatrix&);

void Jacobi(const SymmetricMatrix&, DiagonalMatrix&, SymmetricMatrix&);

void Jacobi(const SymmetricMatrix&, DiagonalMatrix&, Matrix&);

void Jacobi(const SymmetricMatrix&, DiagonalMatrix&, SymmetricMatrix&,
   Matrix&, bool=true);

void EigenValues(const SymmetricMatrix&, DiagonalMatrix&);

void EigenValues(const SymmetricMatrix&, DiagonalMatrix&, SymmetricMatrix&);

void EigenValues(const SymmetricMatrix&, DiagonalMatrix&, Matrix&);

class SymmetricEigenAnalysis
// not implemented yet
{
public:
   SymmetricEigenAnalysis(const SymmetricMatrix&);
private:
   DiagonalMatrix diag;
   DiagonalMatrix offdiag;
   SymmetricMatrix backtransform;
   FREE_CHECK(SymmetricEigenAnalysis)
};

void SortAscending(GeneralMatrix&);

void SortDescending(GeneralMatrix&);


void FFT(const ColumnVector&, const ColumnVector&,
   ColumnVector&, ColumnVector&);

void FFTI(const ColumnVector&, const ColumnVector&,
   ColumnVector&, ColumnVector&);

void RealFFT(const ColumnVector&, ColumnVector&, ColumnVector&);

void RealFFTI(const ColumnVector&, const ColumnVector&, ColumnVector&);

void DCT_II(const ColumnVector&, ColumnVector&);

void DCT_II_inverse(const ColumnVector&, ColumnVector&);

void DST_II(const ColumnVector&, ColumnVector&);

void DST_II_inverse(const ColumnVector&, ColumnVector&);

void DCT(const ColumnVector&, ColumnVector&);

void DCT_inverse(const ColumnVector&, ColumnVector&);

void DST(const ColumnVector&, ColumnVector&);

void DST_inverse(const ColumnVector&, ColumnVector&);


/********************** linear equation solving ****************************/

class LinearEquationSolver : public BaseMatrix
{
   GeneralMatrix* gm;
   int search(const BaseMatrix*) const { return 0; }
   friend class BaseMatrix;
public:
   LinearEquationSolver(const BaseMatrix& bm);
   ~LinearEquationSolver() { delete gm; }
   void CleanUp() { delete gm; } 
   GeneralMatrix* Evaluate(MatrixType) { return gm; }
   // probably should have an error message if MatrixType != UnSp
   NEW_DELETE(LinearEquationSolver)
};


#ifdef use_namespace
}
#endif



#endif
