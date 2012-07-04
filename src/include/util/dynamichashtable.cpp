#include "dynamichashtable.h"

#include <iostream>
#include <sstream>

using namespace std;

//#include "heaplayers.h"
using namespace HL;

class Item {
public:

  Item()
    : _hash (0)
  {}

  Item (unsigned long value,
	string s)
    : _hash (value),
      _str (s)
  {}

  unsigned long hashCode() const {
    return _hash;
  }

  const string& getString() const {
    return _str;
  }

private:
  unsigned long _hash;
  string _str;
};

int
main()
{
  DynamicHashTable<Item, 128> dh;

  for (int i = 0; i < 2000000; i++) {
    stringstream ss;
    ss << "foo:" << i;
    dh.insert (Item (i, ss.str()));
  }

  Item k;
  bool r = dh.get (555555, k);

  if (r) {
    cout << k.getString() << endl;
  }

  return 0;
}
