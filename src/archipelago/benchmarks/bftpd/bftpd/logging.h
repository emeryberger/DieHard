#ifndef LOGGING_H
#define LOGGING_H

extern FILE *logfile;
extern FILE *statuslog;
extern FILE *statuslogforreading;

void log_init();

/* Status codes */
#define SL_UNDEF 0
#define SL_SUCCESS 1
#define SL_FAILURE 2

/* Message type */
#define SL_INFO 1
#define SL_COMMAND 2
#define SL_REPLY 3

void bftpd_statuslog(char type, char type2, char *format, ...);

void bftpd_log(char *format, ...);
void log_end();

#endif
