#ifndef __BFTPD_DIRLIST_H
#define __BFTPD_DIRLIST_H

struct hidegroup {
    int data;
    struct hidegroup *next;
};

void hidegroups_init();
void hidegroups_end();
void bftpd_stat(char *name, FILE *client);
void dirlist(char *name, FILE *client, char verbose);

#endif
