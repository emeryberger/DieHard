#ifndef __BFTPD_MAIN_H
#define __BFTPD_MAIN_H


#define TRUE 1
#define FALSE 0

#include <sys/types.h>

struct bftpd_childpid {
	pid_t pid;
	int sock;
};

extern int global_argc;
extern char **global_argv;
extern struct sockaddr_in name;
extern FILE *passwdfile, *groupfile, *devnull;
extern char *remotehostname;
extern struct sockaddr_in remotename;
extern int control_timeout, data_timeout;
extern int alarm_type;

/* Command line options */
char *configpath;
int daemonmode;

/* scripts to run before and after writing to the file system */
char *pre_write_script;
char *post_write_script;


void print_file(int number, char *filename);

#endif
