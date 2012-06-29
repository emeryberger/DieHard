#include <iostream>
#include <pthread.h>

using namespace std;
#include "socket.h"

ServerSocket ss ("127.0.0.1", 11111);
ClientSocket ll ("127.0.0.1", 11111);

void * write (void * ) {
  ll.connect();
  char buf[10];
  memset (buf, '\0', 10);
  ll.write ("hello", strlen("hello")+1);
  ll.read (buf, 10);
  cout << "read " << buf << endl;
  return NULL;
}

void * read (void * ) {
  ss.serve();
  Socket * s = ss.newConnection();
  char buf[10];
  s->read (buf, 6);
  cout << "server got " << buf << endl;
  s->write ("yo yo yo", strlen("yo yo yo")+1);
  return NULL;
}

main()
{

  pthread_t t1, t2;
  pthread_create (&t1, 0, read, NULL);
  pthread_create (&t2, 0, write, NULL);
  
  pthread_join (t1, NULL);
  pthread_join (t2, NULL);

  cout << "out." << endl;
}
