//$$ newmat1.cpp   Matrix type functions

// Copyright (C) 1991,2,3,4: R B Davies



#include "newmat.h"

#ifdef use_namespace
namespace NEWMAT {
#endif



/************************* MatrixType functions *****************************/


// all operations to return MatrixTypes which correspond to valid types
// of matrices.
// Eg: if it has the Diagonal attribute, then it must also have
// Upper, Lower, Band and Symmetric.


MatrixType MatrixType::operator*(const MatrixType& mt) const
{
   int a = attribute & mt.attribute & ~Symmetric;
   a |= (a & Diagonal) * 31;                   // recognise diagonal
   return MatrixType(a);
}

MatrixType MatrixType::SP(const MatrixType& mt) const
// elementwise product
// Lower, Upper, Diag, Band if only one is
// Symmetric, Valid (and Real) if both are
// Need to include Lower & Upper => Diagonal
// Will need to include both Skew => Symmetric
{
   int a = ((attribute | mt.attribute) & ~(Symmetric + Valid))
      | (attribute & mt.attribute);
   if ((a & Lower) != 0  &&  (a & Upper) != 0) a |= Diagonal;
   a |= (a & Diagonal) * 31;                   // recognise diagonal
   return MatrixType(a);
}

MatrixType MatrixType::i() const               // type of inverse
{
   int a = attribute & ~(Band+LUDeco);
   a |= (a & Diagonal) * 31;                   // recognise diagonal
   return MatrixType(a);
}

MatrixType MatrixType::t() const
// swap lower and upper attributes
// assume Upper is in bit above Lower
{
   int a = attribute;
   a ^= (((a >> 1) ^ a) & Lower) * 3;
   return MatrixType(a);
}

MatrixType MatrixType::MultRHS() const
{
// reomve symmetric attribute unless diagonal
   return (attribute == Dg) ? Dg : (attribute & ~Symmetric);
}

bool Rectangular(MatrixType a, MatrixType b, MatrixType c)
{
   return
      ((a.attribute | b.attribute | c.attribute) & ~MatrixType::Valid) == 0;
}

const char* MatrixType::Value() const
{
// make a string with the name of matrix with the given attributes
   switch (attribute)
   {
   case Valid:                              return "Rect ";
   case Valid+Symmetric:                    return "Sym  ";
   case Valid+Band:                         return "Band ";
   case Valid+Symmetric+Band:               return "SmBnd";
   case Valid+Upper:                        return "UT   ";
   case Valid+Diagonal+Symmetric+Band+Upper+Lower:
                                            return "Diag ";
   case Valid+Band+Upper:                   return "UpBnd";
   case Valid+Lower:                        return "LT   ";
   case Valid+Band+Lower:                   return "LwBnd";
   default:
      if (!(attribute & Valid))             return "UnSp ";
      if (attribute & LUDeco)
         return (attribute & Band) ?     "BndLU" : "Crout";
                                            return "?????";
   }
}


GeneralMatrix* MatrixType::New(int nr, int nc, BaseMatrix* bm) const
{
// make a new matrix with the given attributes

   Tracer tr("New"); GeneralMatrix* gm;
   switch (attribute)
   {
   case Valid:
      if (nc==1) { gm = new ColumnVector(nr); break; }
      if (nr==1) { gm = new RowVector(nc); break; }
      gm = new Matrix(nr, nc); break;

   case Valid+Symmetric:
      gm = new SymmetricMatrix(nr); break;

   case Valid+Band:
      {
         MatrixBandWidth bw = bm->BandWidth();
         gm = new BandMatrix(nr,bw.lower,bw.upper); break;
      }

   case Valid+Symmetric+Band:
      gm = new SymmetricBandMatrix(nr,bm->BandWidth().lower); break;

   case Valid+Upper:
      gm = new UpperTriangularMatrix(nr); break;

   case Valid+Diagonal+Symmetric+Band+Upper+Lower:
      gm = new DiagonalMatrix(nr); break;

   case Valid+Band+Upper:
      gm = new UpperBandMatrix(nr,bm->BandWidth().upper); break;

   case Valid+Lower:
      gm = new LowerTriangularMatrix(nr); break;

   case Valid+Band+Lower:
      gm = new LowerBandMatrix(nr,bm->BandWidth().lower); break;

   default:
      Throw(ProgramException("Invalid matrix type"));
   }
   
   MatrixErrorNoSpace(gm); gm->Protect(); return gm;
}


#ifdef use_namespace
}
#endif

