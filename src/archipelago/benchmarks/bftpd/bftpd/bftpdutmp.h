#ifndef __BFTPD_BFTPDUTMP_H
#define __BFTPD_BFTPDUTMP_H

#include "commands.h"
#include <sys/types.h>

extern FILE *bftpdutmp;

struct bftpdutmp {
    char bu_type;
    pid_t bu_pid;
    char bu_name[USERLEN + 1];
    char bu_host[256];
    time_t bu_time;
};

void bftpdutmp_init();
void bftpdutmp_end();
void bftpdutmp_log(char type);
char bftpdutmp_pidexists(pid_t pid);
int bftpdutmp_usercount(char *username);

/* Count logins from the same machine. */
int bftpdutmp_dup_ip_count(char *ip_address); 

/* Remove a log entry of a client
matching the PID passed. This
makes it look like the client logged
out.
-- Jesse
*/
void bftpdutmp_remove_pid(int pid);

#endif

