//$$ sort.cpp                            Sorting

// Copyright (C) 1991,2,3,4: R B Davies

#define WANT_MATH

#include "include.h"

#include "newmatap.h"

#ifdef use_namespace
namespace NEWMAT {
#endif


/******************************** Quick sort ********************************/

// Quicksort.
// Essentially the method described in Sedgewick's algorithms in C++
// My version is still partially recursive, unlike Segewick's, but the
// smallest segment of each split is used in the recursion, so it should
// not overlead the stack.

// If the process does not seems to be converging an exception is thrown.


#define DoSimpleSort 17            // when to switch to insert sort
#define MaxDepth 50                // maximum recursion depth

static void MyQuickSortDescending(Real* first, Real* last, int depth);
static void InsertionSortDescending(Real* first, const int length,
   int guard);
static Real SortThreeDescending(Real* a, Real* b, Real* c);

static void MyQuickSortAscending(Real* first, Real* last, int depth);
static void InsertionSortAscending(Real* first, const int length,
   int guard);


void SortDescending(GeneralMatrix& GM)
{

   Tracer et("QuickSortDescending");

   Real* data = GM.Store(); int max = GM.Storage();

   if (max > DoSimpleSort) MyQuickSortDescending(data, data + max - 1, 0);
   InsertionSortDescending(data, max, DoSimpleSort);

}

static Real SortThreeDescending(Real* a, Real* b, Real* c)
{
   // sort *a, *b, *c; return *b; optimise for already sorted
   if (*a >= *b)
   {
      if (*b >= *c) return *b;
      else if (*a >= *c) { Real x = *c; *c = *b; *b = x; return x; }
      else { Real x = *a; *a = *c; *c = *b; *b = x; return x; }
   }
   else if (*c >= *b) { Real x = *c; *c = *a; *a = x; return *b; }
   else if (*a >= *c) { Real x = *a; *a = *b; *b = x; return x; }
   else { Real x = *c; *c = *a; *a = *b; *b = x; return x; }
}

static void InsertionSortDescending(Real* first, const int length,
   int guard)
// guard gives the length of the sequence to scan to find first
// element (eg = length)
{
   if (length <= 1) return;

   // scan for first element
   Real* f = first; Real v = *f; Real* h = f;
   if (guard > length) guard = length; int i = guard - 1;
   while (i--) if (v < *(++f)) { v = *f; h = f; }
   *h = *first; *first = v;

   // do the sort
   i = length - 1; f = first;
   while (i--)
   {
      Real* g = f++; h = f; v = *h;
      while (*g < v) *h-- = *g--;
      *h = v;
   }
}

static void MyQuickSortDescending(Real* first, Real* last, int depth)
{
   for (;;)
   {
      const int length = last - first + 1;
      if (length < DoSimpleSort) return;
      if (depth++ > MaxDepth)
         Throw(ConvergenceException("QuickSortDescending fails: "));
      Real* centre = first + length/2;
      const Real test = SortThreeDescending(first, centre, last);
      Real* f = first; Real* l = last;
      for (;;)
      {
         while (*(++f) > test) {}
         while (*(--l) < test) {}
         if (l <= f) break;
         const Real temp = *f; *f = *l; *l = temp;
      }
      if (f > centre) { MyQuickSortDescending(l+1, last, depth); last = f-1; }
      else { MyQuickSortDescending(first, f-1, depth); first = l+1; }
   }
}

void SortAscending(GeneralMatrix& GM)
{

   Tracer et("QuickSortAscending");

   Real* data = GM.Store(); int max = GM.Storage();

   if (max > DoSimpleSort) MyQuickSortAscending(data, data + max - 1, 0);
   InsertionSortAscending(data, max, DoSimpleSort);

}

static void InsertionSortAscending(Real* first, const int length,
   int guard)
// guard gives the length of the sequence to scan to find first
// element (eg guard = length)
{
   if (length <= 1) return;

   // scan for first element
   Real* f = first; Real v = *f; Real* h = f;
   if (guard > length) guard = length; int i = guard - 1;
   while (i--) if (v > *(++f)) { v = *f; h = f; }
   *h = *first; *first = v;

   // do the sort
   i = length - 1; f = first;
   while (i--)
   {
      Real* g = f++; h = f; v = *h;
      while (*g > v) *h-- = *g--;
      *h = v;
   }
}
static void MyQuickSortAscending(Real* first, Real* last, int depth)
{
   for (;;)
   {
      const int length = last - first + 1;
      if (length < DoSimpleSort) return;
      if (depth++ > MaxDepth)
         Throw(ConvergenceException("QuickSortAscending fails: "));
      Real* centre = first + length/2;
      const Real test = SortThreeDescending(last, centre, first);
      Real* f = first; Real* l = last;
      for (;;)
      {
         while (*(++f) < test) {}
         while (*(--l) > test) {}
         if (l <= f) break;
         const Real temp = *f; *f = *l; *l = temp;
      }
      if (f > centre) { MyQuickSortAscending(l+1, last, depth); last = f-1; }
      else { MyQuickSortAscending(first, f-1, depth); first = l+1; }
   }
}

#ifdef use_namespace
}
#endif

