// -*- C++ -*-

#ifndef _IPCONNECTION_H_
#define _IPCONNECTION_H_

#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <sstream>

using namespace std;

class Socket {
public:
  Socket (char * str, int port)
    : _port (port)
    {
      _name = str;
      _name += ':';
      stringstream b;
      b << _port;
      _name += b.str();
      // printf ("socketing up %s on port %d (%s)\n", str, _port, _name.c_str());
      socketfd = socket (AF_INET, SOCK_STREAM, 0);
      if (socketfd < 0) {
	printf ("DOH! - SOCKET FAILURE.\n");
      }
      int on = 1;
      int status = setsockopt( socketfd,
			       SOL_SOCKET,
			       SO_REUSEADDR, /* allow local address reuse */
			       (void *) &on, 
			       sizeof (on));
      connection = gethostbyname (str);
      if (connection == NULL) {
	printf ("DOH! - GETHOSTBYNAME FAILURE.\n");
      }
      memset (&machineName, 0, sizeof(machineName));
      machineName.sin_family = AF_INET;
      machineName.sin_port = htons(_port);
      memcpy (&machineName.sin_addr,
	      connection->h_addr_list[0],
	      connection->h_length);
    }

  virtual ~Socket (void) {
    close();
  }

  const char * name (void) {
    return _name.c_str();
  }

  void write (const char * buf, size_t len) {
    //    printf ("---write to %s ----\n", inet_ntoa(machineName.sin_addr));
    ::write (socketfd, buf, len);
    // printf ("---wrote ----\n");
    for (int i = 0; i < len; i++) {
      fputc (buf[i], stdout);
    }
    // printf ("-----\n");
  }

  int read (char * buf, int len) {
    // printf ("---read from %s ----\n", inet_ntoa(machineName.sin_addr));
    int r = ::recv (socketfd, buf, len, 0);
    for (int i = 0; i < r; i++) {
      fputc (buf[i], stdout);
    }
    // printf ("-----\n");
    return r;
  }
  
  void close (void) {
    ::close (socketfd);
  }

#if 0
  Socket (const Socket& s) {
  }
#endif


  struct hostent * connection;
  struct sockaddr_in machineName;
  int socketfd;

  int _port;

private:
  string _name;
};


class ServerSocket : public Socket {
public:
  ServerSocket (char * str, int port)
    : Socket (str, port)
  {}

  void serve (void) {
    // printf ("waiting. ");
    // printf ("\n");
    int r = bind (socketfd, (struct sockaddr *) &machineName, sizeof(machineName));
    if (r < 0) {
      printf ("DOH - BIND FAILURE.\n");
      perror ("");
    }
    r = listen (socketfd, 5);
    if (r < 0) {
      printf ("DOH! - LISTEN FAILURE.\n");
    }
  }

  Socket * newConnection (void) {
    socklen_t len = sizeof(machineName);
    int newfd = ::accept (socketfd, (struct sockaddr *) &machineName, &len);
    if (newfd < 0) {
      printf ("DOH! - ACCEPT FAILURE.\n");
      perror ("woo");
      return NULL;
    }
    //    printf ("client = %s\n", ...);
    char * name = strdup(this->name());
    char * p = strchr(name, ':');
    *p = '\0';
    // printf ("name for new socket = %s, port = %d\n", name, _port);
    Socket * newSock = new Socket (name, _port);
    newSock->socketfd = newfd;
    return newSock;
  }
};


class ClientSocket : public Socket {
public:
  ClientSocket (char * str, int port)
    : Socket (str, port)
  {}

  void connect (void) {
    // printf ("awaiting connection. ");
    // printf ("\n");

    int r = ::connect (socketfd, (struct sockaddr *) &machineName, (socklen_t) sizeof(machineName));
    if (r < 0) {
      printf ("NO CONNECTEE.\n");
    }
  }
};

#endif
