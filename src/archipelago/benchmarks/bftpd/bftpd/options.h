#ifndef __BFTPD_OPTIONS_H
#define __BFTPD_OPTIONS_H

#include "commands.h"
#include "mypaths.h"

#include <pwd.h>
#include <grp.h>


/*
Define some default options
*/

#define CONTROL_TIMEOUT 300
#define DATA_TIMEOUT 300
#define XFER_BUFSIZE 4096
#define XFER_DELAY 0
#define DEFAULT_PORT 21

struct bftpd_option {
  char *name, *value;
  struct bftpd_option *next;
};
struct list_of_struct_passwd {
    struct passwd pwd;
    struct list_of_struct_passwd *next;
};
struct list_of_struct_group {
    struct group grp;
    struct list_of_struct_group *next;
};

struct directory {
    char *path;
    struct bftpd_option *options;
    struct directory *next;
};
struct global {
    struct bftpd_option *options;
    struct directory *directories;
};
struct group_of_users {
    struct list_of_struct_passwd *users;
    struct list_of_struct_group *groups;
    char *temp_members;
    struct bftpd_option *options;
    struct directory *directories;
    struct group_of_users *next;
};
struct user {
    char *name;
    struct bftpd_option *options;
    struct directory *directories;
    struct user *next;
};

extern struct group_of_users *config_groups;
extern struct user *config_users;

void expand_groups();
void config_init();
char *config_getoption(char *name);
char *config_getoption_reread( char *find_me );
void config_end();
void Reread_Config_File();

#endif
