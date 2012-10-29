#ifndef __BFTPD_COMMANDS_ADMIN_H
#define __BFTPD_COMMANDS_ADMIN_H

void command_adminlogin(char *params);
void command_admingetconf(char *params);
void command_adminstatus(char *params);
void command_adminlog(char *params);
void command_adminstoplog(char *params);
void command_adminquit(char *params);
int admin_parsecmd(char *str);

struct admin_command {
  char *name;
  void (*function)(char *);
};

#endif

