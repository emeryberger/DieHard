//$$ boolean.h                       bool class

// This is for compilers that don't have bool automatically defined

#ifndef bool_LIB
#define bool_LIB 0

#ifdef use_namespace
namespace RBD_COMMON {
#endif


#if !defined(_WIN32) && !defined(__GNUC__)
class bool
{
	int value;
public:
	bool(const int b) { value = b ? 1 : 0; }
	bool(const void* b) { value = b ? 1 : 0; }
	bool() {}
	operator int() const { return value; }
	int operator!() const { return !value; }
};

const bool true = 1;
const bool false = 0;
#endif


// version for some older versions of gnu g++
//#define false 0
//#define true 1

#ifdef use_namespace
}
#endif



#endif
