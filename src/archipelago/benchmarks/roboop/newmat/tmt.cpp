#define WANT_STREAM

#include "include.h"

#include "newmat.h"

#ifdef use_namespace
using namespace NEWMAT;
#endif


/**************************** test program ******************************/

class PrintCounter
{
   int count;
   char* s;
public:
   ~PrintCounter();
   PrintCounter(char * sx) : count(0), s(sx) {}
   void operator++() { count++; }
};

PrintCounter PCZ("Number of non-zero matrices (should be 1) = ");
PrintCounter PCN("Number of matrices tested                 = ");

PrintCounter::~PrintCounter()
{ cout << s << count << "\n"; }


void Print(const Matrix& X)
{
   ++PCN;
   cout << "\nMatrix type: " << X.Type().Value() << " (";
   cout << X.Nrows() << ", ";
   cout << X.Ncols() << ")\n\n";
   if (X.IsZero()) { cout << "All elements are zero\n" << flush; return; }
   int nr=X.Nrows(); int nc=X.Ncols();
   for (int i=1; i<=nr; i++)
   {
      for (int j=1; j<=nc; j++)  cout << X(i,j) << "\t";
      cout << "\n";
   }
   cout << flush; ++PCZ;
}

void Print(const UpperTriangularMatrix& X)
{
   ++PCN;
   cout << "\nMatrix type: " << X.Type().Value() << " (";
   cout << X.Nrows() << ", ";
   cout << X.Ncols() << ")\n\n";
   if (X.IsZero()) { cout << "All elements are zero\n" << flush; return; }
   int nr=X.Nrows(); int nc=X.Ncols();
   for (int i=1; i<=nr; i++)
   {
      int j;
      for (j=1; j<i; j++) cout << "\t";
      for (j=i; j<=nc; j++)  cout << X(i,j) << "\t";
      cout << "\n";
   }
   cout << flush; ++PCZ;
}

void Print(const DiagonalMatrix& X)
{
   ++PCN;
   cout << "\nMatrix type: " << X.Type().Value() << " (";
   cout << X.Nrows() << ", ";
   cout << X.Ncols() << ")\n\n";
   if (X.IsZero()) { cout << "All elements are zero\n" << flush; return; }
   int nr=X.Nrows(); int nc=X.Ncols();
   for (int i=1; i<=nr; i++)
   {
      for (int j=1; j<i; j++) cout << "\t";
      if (i<=nc) cout << X(i,i) << "\t";
      cout << "\n";
   }
   cout << flush; ++PCZ;
}

void Print(const SymmetricMatrix& X)
{
   ++PCN;
   cout << "\nMatrix type: " << X.Type().Value() << " (";
   cout << X.Nrows() << ", ";
   cout << X.Ncols() << ")\n\n";
   if (X.IsZero()) { cout << "All elements are zero\n" << flush; return; }
   int nr=X.Nrows(); int nc=X.Ncols();
   for (int i=1; i<=nr; i++)
   {
      int j;
      for (j=1; j<i; j++) cout << X(j,i) << "\t";
      for (j=i; j<=nc; j++)  cout << X(i,j) << "\t";
      cout << "\n";
   }
   cout << flush; ++PCZ;
}

void Print(const LowerTriangularMatrix& X)
{
   ++PCN;
   cout << "\nMatrix type: " << X.Type().Value() << " (";
   cout << X.Nrows() << ", ";
   cout << X.Ncols() << ")\n\n";
   if (X.IsZero()) { cout << "All elements are zero\n" << flush; return; }
   int nr=X.Nrows();
   for (int i=1; i<=nr; i++)
   {
      for (int j=1; j<=i; j++) cout << X(i,j) << "\t";
      cout << "\n";
   }
   cout << flush; ++PCZ;
}


void Clean(Matrix& A, Real c)
{
   int nr = A.Nrows(); int nc = A.Ncols();
   for (int i=1; i<=nr; i++)
   {
      for (int j=1; j<=nc; j++)
      { Real a = A(i,j); if ((a < c) && (a > -c)) A(i,j) = 0.0; }
   }
}

void Clean(DiagonalMatrix& A, Real c)
{
   int nr = A.Nrows();
   for (int i=1; i<=nr; i++)
   { Real a = A(i,i); if ((a < c) && (a > -c)) A(i,i) = 0.0; }
}

void PentiumCheck(Real N, Real D)
{
   Real R = N / D;
   R = R * D - N;
   if ( R > 1 || R < -1)
      cout << "Pentium error detected: % error = " << 100 * R / N << "\n";
}



//*************************** main program **********************************

void TestTypeAdd();                            // test +
void TestTypeMult();                           // test *
void TestTypeSP();                             // test SP
void TestTypeOrder();                          // test >=


void trymat1(); void trymat2(); void trymat3();
void trymat4(); void trymat5(); void trymat6();
void trymat7(); void trymat8(); void trymat9();
void trymata(); void trymatb(); void trymatc();
void trymatd(); void trymate(); void trymatf();
void trymatg(); void trymath(); void trymati();
void trymatj(); void trymatk();

main()
{
   Real* s1; Real* s2; Real* s3; Real* s4;
   cout << "\nBegin test\n";   // Forces cout to allocate memory at beginning
   // Throw exception to set up exception buffer
   Try { Throw(Exception("Just a dummy\n")); }
   CatchAll {}
   { Matrix A1(40,200); s1 = A1.Store(); }
   { Matrix A1(1,1); s3 = A1.Store(); }
   {
      Tracer et("Matrix test program");

      Matrix A(25,150);
      {
	 int i;
	 RowVector A(8);
	 for (i=1;i<=7;i++) A(i)=0.0; A(8)=1.0;
	 Print(A);
      }
      cout << "\n";

      TestTypeAdd(); TestTypeMult(); TestTypeSP(); TestTypeOrder();

      Try { 

         trymat1();
         trymat2();
         trymat3();
         trymat4();
         trymat5();
         trymat6();
         trymat7();
         trymat8();
         trymat9();
         trymata();
         trymatb();
         trymatc();
         trymatd();
         trymate();
         trymatf();
         trymatg();
         trymath();
         trymati();
         trymatj();
         trymatk();

         cout << "\nEnd of tests\n";
      }
      CatchAll
      {
         cout << "\nTest program fails - exception generated\n\n";
         cout << Exception::what();
      }
   }

   { Matrix A1(40,200); s2 = A1.Store(); }
   cout << "\n(The following memory checks are probably not valid with all\n";
   cout << "compilers - see documentation)\n";
   cout << "\nChecking for lost memory: "
      << (unsigned long)s1 << " " << (unsigned long)s2 << " ";
   if (s1 != s2) cout << " - error\n"; else cout << " - ok\n\n";
   { Matrix A1(1,1); s4 = A1.Store(); }
   cout << "\nChecking for lost memory: "
      << (unsigned long)s3 << " " << (unsigned long)s4 << " ";
   if (s3 != s4) cout << " - error\n"; else cout << " - ok\n\n";

   // check for Pentium bug
   PentiumCheck(4195835L,3145727L);
   PentiumCheck(5244795L,3932159L);

#ifdef DO_FREE_CHECK
   FreeCheck::Status();
#endif
   return 0;
}




//************************ test type manipulation **************************/


// These functions may cause problems for Glockenspiel 2.0c; they are used
// only for testing so you can delete them


void TestTypeAdd()
{
   MatrixType list[9];
   list[0] = MatrixType::UT;
   list[1] = MatrixType::LT;
   list[2] = MatrixType::Rt;
   list[3] = MatrixType::Sm;
   list[4] = MatrixType::Dg;
   list[5] = MatrixType::BM;
   list[6] = MatrixType::UB;
   list[7] = MatrixType::LB;
   list[8] = MatrixType::SB;

   cout << "+     ";
   int i;
   for (i=0; i<MatrixType::nTypes(); i++) cout << list[i].Value() << " ";
   cout << "\n";
   for (i=0; i<MatrixType::nTypes(); i++)
	{
		cout << list[i].Value() << " ";
      for (int j=0; j<MatrixType::nTypes(); j++)
	 cout << (list[j]+list[i]).Value() << " ";
      cout << "\n";
   }
   cout << "\n";
}

void TestTypeMult()
{
   MatrixType list[9];
   list[0] = MatrixType::UT;
   list[1] = MatrixType::LT;
   list[2] = MatrixType::Rt;
   list[3] = MatrixType::Sm;
   list[4] = MatrixType::Dg;
   list[5] = MatrixType::BM;
   list[6] = MatrixType::UB;
   list[7] = MatrixType::LB;
   list[8] = MatrixType::SB;

   cout << "*     ";
   int i;
   for (i=0; i<MatrixType::nTypes(); i++)
      cout << list[i].Value() << " ";
   cout << "\n";
   for (i=0; i<MatrixType::nTypes(); i++)
   {
		cout << list[i].Value() << " ";
      for (int j=0; j<MatrixType::nTypes(); j++)
	 cout << (list[j]*list[i]).Value() << " ";
      cout << "\n";
   }
   cout << "\n";
}

void TestTypeSP()
{
   MatrixType list[9];
   list[0] = MatrixType::UT;
   list[1] = MatrixType::LT;
   list[2] = MatrixType::Rt;
   list[3] = MatrixType::Sm;
   list[4] = MatrixType::Dg;
   list[5] = MatrixType::BM;
   list[6] = MatrixType::UB;
   list[7] = MatrixType::LB;
   list[8] = MatrixType::SB;

   cout << "SP    ";
   int i;
   for (i=0; i<MatrixType::nTypes(); i++)
		cout << list[i].Value() << " ";
   cout << "\n";
   for (i=0; i<MatrixType::nTypes(); i++)
   {
		cout << list[i].Value() << " ";
      for (int j=0; j<MatrixType::nTypes(); j++)
	 cout << (list[j].SP(list[i])).Value() << " ";
      cout << "\n";
   }
   cout << "\n";
}

void TestTypeOrder()
{
   MatrixType list[9];
   list[0] = MatrixType::UT;
   list[1] = MatrixType::LT;
   list[2] = MatrixType::Rt;
   list[3] = MatrixType::Sm;
   list[4] = MatrixType::Dg;
   list[5] = MatrixType::BM;
   list[6] = MatrixType::UB;
   list[7] = MatrixType::LB;
   list[8] = MatrixType::SB;

   cout << ">=    ";
   int i;
   for (i = 0; i<MatrixType::nTypes(); i++)
      cout << list[i].Value() << " ";
   cout << "\n";
   for (i=0; i<MatrixType::nTypes(); i++)
   {
      cout << list[i].Value() << " ";
      for (int j=0; j<MatrixType::nTypes(); j++)
	 cout << ((list[j]>=list[i]) ? "Yes   " : "No    ");
      cout << "\n";
   }
   cout << "\n";
}


