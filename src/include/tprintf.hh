#ifndef TPRINTF_H
#define TPRINTF_H

#pragma once

// Written by Emery Berger

#include <unistd.h>
#include <string.h>
#include <cmath>
#include <cstdint>

using namespace std;

namespace tprintf {
  
  const int MAXBUF = 65536;
  const int FD = 1; // 2

  template <typename T>
  void writeval(T v);


  template <typename T>
  int itoa(char * buf, T v) {
    long n = (long) v;
    auto startbuf = buf;
    if (n < 0) {
      *buf++ = '-';
      n = -n;
    }
    if (n == 0) {
      *buf++ = '0';
      return (int) (buf - startbuf);
    }
    long tens = 1L;
    while (n / (10 * tens)) {
      tens *= 10;
    }
    while (tens) {
      *buf++ = '0' + n / tens;
      n = n - (n / tens) * tens;
      tens /= 10;
    }
    return (int) (buf - startbuf);
  }

  int ftoa(char * buf, double n, int decimalPlaces = 4) {
    // Extract integer part
    auto ipart = (long) n;
  
    // Extract floating part
    auto fpart = n - (double) ipart;
    if (fpart < 0.0) {
      fpart = -fpart;
    }
  
    // convert integer part to string
    int i = itoa(buf, ipart);
  
    if (decimalPlaces > 0) {
      buf[i] = '.';
      auto multiple = pow(10, decimalPlaces);
      fpart = fpart * multiple;
      multiple /= 10;
      while ((fpart < multiple) && (decimalPlaces > 0)) {
	buf[++i] = '0';
	multiple /= 10;
	decimalPlaces--;
      }
      if (fpart > 0) {
	i = i + itoa(buf + i + 1, (long) fpart) + 1;
      }
    }
    return i;
  }

  void writeval(double n) {
    char buf[MAXBUF];
    int len = ftoa(buf, n);
    write(FD, buf, len);
  }
  
  void writeval(const char * str) {
    write(FD, str, strlen(str));
  }

  void writeval(const char c) {
    char buf[1];
    buf[0] = c;
    write(FD, buf, 1);
  }

  void writeval(uint64_t n) {
    char buf[MAXBUF];
    int len = itoa(buf, n);
    write(FD, buf, len);
  }

  template <class T>
  void writeval(T n) {
    char buf[MAXBUF];
    int len = itoa(buf, n);
    write(FD, buf, len);
  }

  void tprintf(const char* format) // base function
  {
    writeval(format);
  }

  template<typename T, typename... Targs>
  void tprintf(const char * format, T value, Targs... Fargs)
  {
    for ( ; *format != '\0'; format++ ) {
      if ( *format == '@' ) {
	if ( *(format + 1) == '@') {
	  writeval("@");
	  format = format+2;
	} else {
	  writeval(value);
	  tprintf(format+1, Fargs...);
	  return;
	}
      }
      writeval(*format);
    }
  }

}


#endif
