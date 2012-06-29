#ifndef _IFCLASS_H_
#define _IFCLASS_H_

template <bool b, class a, class c>
class IfClass;

template <class a, class b>
class IfClass<true, a, b> : public a {};

template <class a, class b>
class IfClass<false, a, b> : public b {};


#endif
