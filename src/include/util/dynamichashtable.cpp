#include "dynamichashtable.h"

#include <iostream>
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
  
  dh.insert (Item (1, "woo"));
  dh.insert (Item (1, "bag"));
  dh.insert (Item (2, "boo"));
  dh.insert (Item (3, "doo"));
  dh.insert (Item (4, "woom"));
  dh.insert (Item (5, "bagm"));
  dh.insert (Item (6, "boom"));
  dh.insert (Item (7, "doom"));
  for (int i = 0; i < 100; i++) {
    dh.insert (Item (i + 7, "moop"));
  }

  Item k;
  bool r = dh.get (1, k);

  if (r) {
    cout << k.getString() << endl;
  }

  return 0;
}
