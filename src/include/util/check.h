// -*- C++ -*-

#ifndef CHECK_H
#define CHECK_H

template <class C>
class Check {
public:

  Check (C item)
    : _item (item)
  {
#ifndef NDEBUG
    _item->check();
#endif
  }

  ~Check() {
#ifndef NDEBUG
    _item->check();
#endif
  }

private:

  const C _item;

};

#endif
