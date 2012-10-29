#ifndef __BFTPD_MYSTRING_H
#define __BFTPD_MYSTRING_H

int pos(char *, char *);
void cutto(char *, int);
void mystrncpy(char *, char *, int);
int replace(char *, char *, char *);
char *readstr();
int int_from_list(char *list, int n);

#endif
