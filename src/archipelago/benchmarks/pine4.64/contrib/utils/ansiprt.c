/*
 * ansiprt.c
 *
 * Simple filter to wrap ANSI media copy escape sequences around 
 * text on stdin.  Writes /dev/tty to get around things that might be
 * trapping stdout.  This is actually a feature because it was written
 * to be used with pine's personal print option set up to take "enscript"
 * output and send it displayward to be captured/printed to a postscript 
 * device.  Pine, of course, uses popen() to invoke the personal print
 * command, and interprets stdout as diagnostic messages from the command.
 *
 * Michael Seibel, mikes@cac.washington.edu
 *
 * 21 Apr 92
 *
 */
#include <stdio.h>
#include <sys/file.h>

#define	BUFSIZ	8192

main(argc, argv)
int  argc;
char **argv;
{
    char c[BUFSIZ];
    int  n, d;
    int  ctrld = 0;

    if(argc > 1){
        n = 0;
	while(argc > ++n){
	    if(argv[n][0] == '-'){
		switch(argv[n][1]){
		  case 'd':
		    ctrld++;
		    break;
		  default :
		    fprintf(stderr,"unknown option: %c\n", argv[n][1]);
		    break;
		}
	    }
        }
    }

    if((d=open("/dev/tty",O_WRONLY)) < 0){
        perror("/dev/tty");
	exit(1);
    }

    write(d,"\033[5i", 4);
    while((n=read(0, c, BUFSIZ)) > 0)
	write(d, c, n);

    if(ctrld)
	write(d, "\004", 1);

    write(d,"\033[4i", 4);
    close(d);
}
