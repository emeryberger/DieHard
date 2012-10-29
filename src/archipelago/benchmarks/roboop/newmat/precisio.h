//$$ precisio.h                          floating point constants

#ifndef PRECISION_LIB
#define PRECISION_LIB 0

#ifdef use_namespace
namespace NEWMAT {
#endif


#ifndef SystemV                    // if there is float.h


#ifdef USING_FLOAT


class FloatingPointPrecision
{
public:
   static int Dig()
      { return FLT_DIG; }        // number of decimal digits or precision

   static Real Epsilon()
      { return FLT_EPSILON; }    // smallest number such that 1+Eps!=Eps

   static int Mantissa()
      { return FLT_MANT_DIG; }   // bits in mantisa

   static Real Maximum()
      { return FLT_MAX; }        // maximum value

   static int MaximumDecimalExponent()
      { return FLT_MAX_10_EXP; } // maximum decimal exponent

   static int MaximumExponent()
      { return FLT_MAX_EXP; }    // maximum binary exponent

   static Real LnMaximum()
      { return (Real)log(Maximum()); } // natural log of maximum

   static Real Minimum()
      { return FLT_MIN; }        // minimum positive value

   static int MinimumDecimalExponent()
      { return FLT_MIN_10_EXP; } // minimum decimal exponent

   static int MinimumExponent()
      { return FLT_MIN_EXP; }    // minimum binary exponent

   static Real LnMinimum()
      { return (Real)log(Minimum()); } // natural log of minimum

   static int Radix()
      { return FLT_RADIX; }      // exponent radix

   static int Rounds()
      { return FLT_ROUNDS; }     // addition rounding (1 = does round)

};

#endif


#ifdef USING_DOUBLE

class FloatingPointPrecision
{
public:

   static int Dig()
      { return DBL_DIG; }        // number of decimal digits or precision

   static Real Epsilon()
      { return DBL_EPSILON; }    // smallest number such that 1+Eps!=Eps

   static int Mantissa()
      { return DBL_MANT_DIG; }   // bits in mantisa

   static Real Maximum()
      { return DBL_MAX; }        // maximum value

   static int MaximumDecimalExponent()
      { return DBL_MAX_10_EXP; } // maximum decimal exponent

   static int MaximumExponent()
      { return DBL_MAX_EXP; }    // maximum binary exponent

   static Real LnMaximum()
      { return (Real)log(Maximum()); } // natural log of maximum

   static Real Minimum()
   {
//#ifdef __BCPLUSPLUS__
//       return 2.225074e-308;     // minimum positive value
//#else
       return DBL_MIN;
//#endif
   }

   static int MinimumDecimalExponent()
      { return DBL_MIN_10_EXP; } // minimum decimal exponent

   static int MinimumExponent()
      { return DBL_MIN_EXP; }    // minimum binary exponent

   static Real LnMinimum()
      { return (Real)log(Minimum()); } // natural log of minimum


   static int Radix()
      { return FLT_RADIX; }      // exponent radix

   static int Rounds()
      { return FLT_ROUNDS; }     // addition rounding (1 = does round)

};

#endif

#endif

#ifdef SystemV                    // if there is no float.h

#ifdef USING_FLOAT

class FloatingPointPrecision
{
public:

   static Real Epsilon()
      { return pow(2.0,1-FSIGNIF); }  // smallest number such that 1+Eps!=Eps

   static Real Maximum()
      { return MAXFLOAT; }            // maximum value

   static Real LnMaximum()
      { return (Real)log(Maximum()); }  // natural log of maximum

   static Real Minimum()
      { return MINFLOAT; }            // minimum positive value

   static Real LnMinimum()
      { return (Real)log(Minimum()); }  // natural log of minimum

};

#endif


#ifdef USING_DOUBLE

class FloatingPointPrecision
{
public:

   static Real Epsilon()
      { return pow(2.0,1-DSIGNIF); }  // smallest number such that 1+Eps!=Eps

   static Real Maximum()
      { return MAXDOUBLE; }           // maximum value

   static Real LnMaximum()
      { return LN_MAXDOUBLE; }        // natural log of maximum

   static Real Minimum()
      { return MINDOUBLE; }

   static Real LnMinimum()
      { return LN_MINDOUBLE; }        // natural log of minimum
};

#endif

#endif

#ifdef use_namespace
}
#endif



#endif
