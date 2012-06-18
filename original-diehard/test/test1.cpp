class Foo {
public:
  Foo (char * i)
    : info (i)
  {}
  char * info;
};


int main()
{
  char buf[255];
  Foo * f = new Foo ("bar");

  // whoops - part of stack, not heap
  delete &buf[4];

  // d'oh, in middle
  delete ((char *) f + 1);

  return 0;
} 
