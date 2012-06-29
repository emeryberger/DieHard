#include <iostream>
using namespace std;

class Foo {
public:
  Foo (char * i)
    : info (i)
  {}
  char * info;
};


int main()
{
  Foo * f = new Foo ("happy");
  Foo * x = f;
  delete f;
  Foo * g = new Foo ("sad");
  
  // dang, dangling pointer
  cout << x->info << endl;
  return 0;
}
