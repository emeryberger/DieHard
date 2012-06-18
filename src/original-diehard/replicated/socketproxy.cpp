#include <stdio.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "socket.h"

#include <algorithm>
using namespace std;

typedef pair<Socket *, Socket *> Connection;

static const unsigned long firstPortNumber = 11001;

static unsigned long nextPortNumber = firstPortNumber;

const int mainPortNumber = 11000;

extern "C" void * readWrite (void * arg) {
  Connection * sp = (Connection *) arg;
  int v = 1;
  const int len = 4096;
  char buf[len];
  printf ("waiting to read.\n");
  while (v > 0) {
    v = sp->first->read (buf, len);
    if (v > 0) {
      printf ("waiting to write.\n");
      sp->second->write (buf, v);
    }
  }
  return NULL;
}

extern "C" void * goToIt (void * arg) {
  Connection * sp = (Connection *) arg;
  ServerSocket * local = (ServerSocket *) (sp->first);
  Socket * remote = sp->second;

  printf ("local = %s, remote = %s\n", local->name(), remote->name());
  Socket * thisLocal = local->newConnection();

  // Spawn the threads.
  Connection * p1 = new Connection (thisLocal, remote);
  Connection * p2 = new Connection (remote, thisLocal);

  pthread_t t1, t2;
  pthread_create (&t1, NULL, readWrite, (void *) &p1);
  pthread_create (&t2, NULL, readWrite, (void *) &p2);

  pthread_join (t1, NULL);
  pthread_join (t2, NULL);

  return NULL;
}


void handleInitialConnection (void) {
  // Connect to client (shimmed) -- should be first thing accessed by
  // bind.
  char hostname[64];
  gethostname(hostname, sizeof(hostname));
  printf ("local = %s\n", hostname);
  ServerSocket server (hostname, mainPortNumber);
  server.serve();

  while (true) {
    // We need to read in the actual destination & port number.
    printf ("waiting for input...\n");
    Socket * s = server.newConnection();
    if (!s) {
      return;
    }
    // Send out the new port number for subsequent connections.
    char buf[255];
    sprintf (buf, "%d\n", nextPortNumber);
    s->write (buf, strlen(buf));

    // Get the intended IP address and port number that we will be
    // proxying.
    char name[255];
    memset (name, '\0', 255);
    int v = s->read (name, 254);
    printf ("intended target = %s\n", name);

    delete s;

    // Split out the name and the port number.
    char * p = strchr ((const char *) name, ':');
    int portnumber;
    if (p) {
      *p = '\0';
      p++;
      portnumber = atoi(p);
    } else {
      portnumber = 0;
    }

    printf ("connection intended for %s at port %d\n", name, portnumber);
    // Now establish the proxy mapping for the actual proxy thread.

    ClientSocket * remote = new ClientSocket (strdup(name), portnumber);
    ServerSocket * local = new ServerSocket (strdup("localhost"), nextPortNumber);
    remote->connect();
    local->serve();

    Connection * p0 = new Connection (local, remote);
    pthread_t t0;
    pthread_create (&t0, NULL, goToIt, (void *) p0);

    nextPortNumber++;
    //    printf ("port number now = %d\n", nextPortNumber);
  }
}


int main (int argc, char * argv[]) 
{
  handleInitialConnection();
}
