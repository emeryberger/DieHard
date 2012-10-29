#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <main.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "logging.h"
#include "options.h"
#include "mystring.h"

FILE *logfile = NULL;
FILE *statuslog = NULL;
FILE *statuslogforreading = NULL;

char log_syslog = 0;

void log_init()
{
    char *foo = config_getoption("LOGFILE");
#ifdef HAVE_SYSLOG_H
	if (!strcasecmp(foo, "syslog")) {
        log_syslog = 1;
		openlog(global_argv[0], LOG_PID, LOG_DAEMON);
	} else
#endif
    if (foo[0])
        if (!(logfile = fopen(foo, "a"))) {
    		control_printf(SL_FAILURE, "421-Could not open log file.\r\n"
    		         "421 Server disabled for security reasons.");
    		exit(1);
    	}
    statuslog = fopen(PATH_STATUSLOG, "a");
    /* This one is for the admin code. */
    statuslogforreading = fopen(PATH_STATUSLOG, "r");
}

/* Status file format:
 * <ID> <Type> <Type2> <String>
 * ID: PID of server process
 * Type: SL_INFO for information, SL_COMMAND for command, SL_REPLY for reply.
 * Type2: If Type = SL_INFO or SL_COMMAND, Type2 is SL_UNDEF. Else,
 * it's SL_SUCCESS for success and SL_FAILURE for error.
 */
void bftpd_statuslog(char type, char type2, char *format, ...)
{
    if (statuslog) {
        va_list val;
        char buffer[1024];
        va_start(val, format);
        vsnprintf(buffer, sizeof(buffer), format, val);
        va_end(val);
        fseek(statuslog, 0, SEEK_END);
        fprintf(statuslog, "%i %i %i %s\n", (int) getpid(), type, type2,
                buffer);
        fflush(statuslog);
    }
}

void bftpd_log(char *format, ...)
{
	va_list val;
	char buffer[1024], timestr[40];
	time_t t;
	va_start(val, format);
	vsnprintf(buffer, sizeof(buffer), format, val);
	va_end(val);
	if (logfile) {
		fseek(logfile, 0, SEEK_END);
		time(&t);
		strcpy(timestr, (char *) ctime(&t));
		timestr[strlen(timestr) - 1] = '\0';
		fprintf(logfile, "%s %s[%i]: %s", timestr, global_argv[0],
				(int) getpid(), buffer);
		fflush(logfile);
	}
#ifdef HAVE_SYSLOG_H
    else if (log_syslog)
        syslog(LOG_DAEMON | LOG_INFO, "%s", buffer);
#endif
}

void log_end()
{
	if (logfile) {
		fclose(logfile);
		logfile = NULL;
	}
#ifdef HAVE_SYSLOG_H
    else if (log_syslog)
		closelog();
#endif
    if (statuslog) {
        fclose(statuslog);
        statuslog = NULL;
    }
}

