#include <stdio.h>
#include <stdlib.h>


#include <iostream>
#include <iomanip>

using namespace std;

int
main()
{
  const int factor = 5;

  cout << "<html><head></head><body><canvas id=\"myCanvas\" width=\"500\" height=\"500\" style=\"border:1px solid #c3c3c3;\">Your browser does not support the canvas element.</canvas><script type=\"text/javascript\">" << endl;

  cout << "var c=document.getElementById(\"myCanvas\"); var ctx=c.getContext(\"2d\");  ctx.fillStyle=\"#FF0000\";" << endl;

  const int count = 100;
  char * arr[count];
  for (int j = 0; j < 100; j++) {
    for (int i = 0; i < count; i++) {
      arr[i] = new char[32];

      //      int color = lrand48() % 256;
      int color = ((size_t) arr[i]) % 256 ;

      // Output the color in hex.
      cout << "ctx.fillStyle=\"#" << hex
	   << setw(2) << setfill('0') << color
	   << setw(2) << setfill('0') << color
	   << setw(2) << setfill('0') << color << "\";" << endl;
      // Now fill a factor X factor rectangle.
      cout << dec;
      cout << "ctx.fillRect(" << j*factor << "," << i*factor << ", " << (j+1)*factor << ", " << (i+1)*factor << ");" << endl;

    }

    for (int i = 0; i < count; i++) {
      delete [] arr[i];
    }
  }

  cout << "</script>" << endl;

cout << "</body></html>" << endl;

  return 0;
}
