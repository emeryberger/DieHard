#ifndef __BFTPD_CWD_H
#define __BFTPD_CWD_H

// Changes the current working directory following symlinks
int bftpd_cwd_chdir(char *dir);

// Returns the current working directory
char *bftpd_cwd_getcwd();

// Makes a relative path absolute following symlinks
char *bftpd_cwd_mappath(char *path);

// Stuff
void bftpd_cwd_init();
void bftpd_cwd_end();

#endif
