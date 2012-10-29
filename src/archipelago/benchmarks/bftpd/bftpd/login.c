#include <config.h>
#include <stdio.h>
#include <pwd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
/* add BSD support */
#include <limits.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef WANT_PAM
#include <security/pam_appl.h>
#endif
#ifdef HAVE_UTMP_H
# include <utmp.h>
# ifdef HAVE_PATHS_H
#  include <paths.h>
#  ifndef _PATH_WTMP
#   define _PATH_WTMP "/dev/null"
#   warning "<paths.h> doesn't set _PATH_WTMP. You can not use wtmp logging"
#   warning "with bftpd."
#  endif
# else
#  define _PATH_WTMP "/dev/null"
#  warning "<paths.h> was not found. You can not use wtmp logging with bftpd."
# endif
#endif
#include <errno.h>
#include <grp.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "cwd.h"
#include "dirlist.h"
#include "mystring.h"
#include "options.h"
#include "login.h"
#include "logging.h"
#include "bftpdutmp.h"
#include "main.h"

#ifdef WANT_PAM
char usepam = 0;
pam_handle_t *pamh = NULL;
#endif

#ifdef HAVE_UTMP_H
FILE *wtmp;
#endif

struct passwd userinfo;
char userinfo_set = 0;

char *mygetpwuid(int uid, FILE * file, char *name)
{
	int _uid;
	char foo[256];
    int i;
	if (file) {
		rewind(file);
        while (fscanf(file, "%255s%*[^\n]\n", foo) != EOF) {
            if ((foo[0] == '#') || (!strchr(foo, ':')) || (strchr(foo, ':') > foo + USERLEN - 1))
                continue;
            i = strchr(foo, ':') - foo;
            strncpy(name, foo, i);
            name[i] = 0;
			sscanf(strchr(foo + i + 1, ':') + 1, "%i", &_uid);
			if (_uid == uid) {
				if (name[0] == '\n')
					cutto(name, 1);
				return name;
			}
		}
	}
	sprintf(name, "%i", uid);
	return name;
}

int mygetpwnam(char *name, FILE * file)
{
	char _name[USERLEN + 1];
	char foo[256];
	int uid, i;
	if (file) {
		rewind(file);
        while (fscanf(file, "%255s%*[^\n]\n", foo) != EOF) {
            if ((foo[0] == '#') || (!strchr(foo, ':')) || (strchr(foo, ':') > foo + USERLEN - 1))
                continue;
            i = strchr(foo, ':') - foo;
            strncpy(_name, foo, i);
            _name[i] = 0;
			sscanf(strchr(foo + i + 1, ':') + 1, "%i", &uid);
			if (_name[0] == '\n')
				cutto(_name, 1);
			if (!strcmp(name, _name))
				return uid;
		}
	}
	return -1;
}

#ifdef HAVE_UTMP_H
void wtmp_init()
{
	if (strcasecmp(config_getoption("LOG_WTMP"), "no")) {
		if (!((wtmp = fopen(_PATH_WTMP, "a"))))
			bftpd_log("Warning: Unable to open %s.\n", _PATH_WTMP);
	}
}

void bftpd_logwtmp(char type)
{
	struct utmp ut;
	if (!wtmp)
		return;
	memset((void *) &ut, 0, sizeof(ut));
#ifdef _HAVE_UT_PID
	ut.ut_pid = getpid();
#endif
	sprintf(ut.ut_line, "ftp%i", (int) getpid());
	if (type) {
#ifdef _HAVE_UT_TYPE
		ut.ut_type = USER_PROCESS;
#endif
		strncpy(ut.ut_name, user, sizeof(ut.ut_name));
#ifdef _HAVE_UT_HOST   
		strncpy(ut.ut_host, remotehostname, sizeof(ut.ut_host));
#endif    
	} else {
#ifdef _HAVE_UT_TYPE
		ut.ut_type = DEAD_PROCESS;
#endif
	}
	time(&(ut.ut_time));
	fseek(wtmp, 0, SEEK_END);
	fwrite((void *) &ut, sizeof(ut), 1, wtmp);
	fflush(wtmp);
}

void wtmp_end()
{
	if (wtmp) {
		if (state >= STATE_AUTHENTICATED)
			bftpd_logwtmp(0);
		fclose(wtmp);
	}
}
#endif

void login_init()
{
    char *foo = config_getoption("INITIAL_CHROOT");
#ifdef HAVE_UTMP_H
	wtmp_init();
#endif
    if (foo[0]) { /* Initial chroot */
        if (chroot(foo) == -1) {
            control_printf(SL_FAILURE, "421 Initial chroot failed.\r\n.");
            exit(1);
        }
    }
}

int bftpd_setuid(uid_t uid)
{
    /* If we must open the data connections from port 20,
     * we have to keep the possibility to regain root privileges */
    if (!strcasecmp(config_getoption("DATAPORT20"), "yes"))
        return seteuid(uid);
    else
        return setuid(uid);
}

int bftpd_login(char *password)
{
	char str[512 + 1];
	char *foo;
	int maxusers;
        char *file_auth;   /* if used, points to file used to auth users */
        char *home_directory = NULL;   /* retrieved from auth_file */
        char *anonymous = NULL;
        unsigned long get_maxusers;

        str[0] = '\0';     /* avoid garbage in str */
        file_auth = config_getoption("FILE_AUTH");

        if (! file_auth[0] )    /* not using auth file */
        {
           // check to see if regular authentication is avail
           #ifndef NO_GETPWNAM
	   if (!getpwnam(user)) {
                control_printf(SL_FAILURE, "421 Login incorrect.");
		exit(0);
           }
           #endif
        }
        /* we are using auth_file */
        else
        {
           home_directory = check_file_password(file_auth, user, password);
           anonymous = config_getoption("ANONYMOUS_USER");
           if (! home_directory)
           {
               if (! strcasecmp(anonymous, "yes") )
                   home_directory = "/";
               else
               {
                  control_printf(SL_FAILURE, "421 Authentication incorrect.");
                  exit(0);
               }
           }
        }

	if (strncasecmp(foo = config_getoption("DENY_LOGIN"), "no", 2)) {
		if (foo[0] != '\0') {
			if (strncasecmp(foo, "yes", 3))
				control_printf(SL_FAILURE, "421-Server disabled.\r\n421 Reason: %s", foo);
			else
				control_printf(SL_FAILURE, "421 Login incorrect.");
			bftpd_log("Login as user '%s' failed: Server disabled.\n", user);
			exit(0);
		}
	}
	get_maxusers = strtoul(config_getoption("USERLIMIT_GLOBAL"), NULL, 10);
        if (get_maxusers <= INT_MAX)
           maxusers = get_maxusers;
        else
        {
           bftpd_log("Error getting max users for GLOBAL in bftpd_login.\n", 0);
           maxusers = 0;
        }
	if ((maxusers) && (maxusers == bftpdutmp_usercount("*"))) {
		control_printf(SL_FAILURE, "421 There are already %i users logged in.", maxusers);
                bftpd_log("Login as user '%s' failed. Too many users on server.\n", user);
		exit(0);
	}
	get_maxusers = strtoul(config_getoption("USERLIMIT_SINGLEUSER"), NULL, 10);
        if (get_maxusers <= INT_MAX)
           maxusers = get_maxusers;
        else
        {
           bftpd_log("error getting max users (SINGLE USER) in bftpd_login.\n", 0);
           maxusers = 0;
        }
	if ((maxusers) && (maxusers == bftpdutmp_usercount(user))) {
		control_printf(SL_FAILURE, "421 User %s is already logged in %i times.", user, maxusers);
                bftpd_log("Login as user '%s' failed. Already logged in %d times.", maxusers);
		exit(0);
	}

        /* Check to see if we should block mulitple logins from the same machine.
           -- Jesse <slicer69@hotmail.com>
        */
        get_maxusers = strtoul( config_getoption("USERLIMIT_HOST"), NULL, 10);
        if (get_maxusers <= INT_MAX)
           maxusers = get_maxusers;
        else
        {
            bftpd_log("Error getting max users per HOST in bftpd_login.\n", 0);
            maxusers = 0;
        }

        if ( (maxusers) && (maxusers == bftpdutmp_dup_ip_count(remotehostname) ) )
        {
            control_printf(SL_FAILURE, "421 Too many connections from your IP address.");
            bftpd_log("Login as user '%s' failed. Already %d connections from %s.\n", user, maxusers, remotehostname);
            exit(0);
        }
       
        /* disable these checks when logging in via auth file */
        if (! file_auth[0] ) 
        {
            #ifndef NO_GETPWNAM
	    if(checkuser() || checkshell()) {
		control_printf(SL_FAILURE, "421 Login incorrect.");
		exit(0);
	    }
            #endif
        }

        /* do not do this check when we are using auth_file */
        if (! file_auth[0] )
        {
            #ifndef NO_GETPWNAM
	    if (checkpass(password))
		return 1;
            #endif
        }

	if (strcasecmp((char *) config_getoption("RATIO"), "none")) {
		sscanf((char *) config_getoption("RATIO"), "%i/%i",
			   &ratio_send, &ratio_recv);
	}

        /* do these checks if logging in via normal methods */
        if (! file_auth[0])
        {
	     strcpy(str, config_getoption("ROOTDIR"));
	     if (!str[0])
		strcpy(str, "%h");
	     replace(str, "%u", userinfo.pw_name);
	     replace(str, "%h", userinfo.pw_dir);
	     if (!strcasecmp(config_getoption("RESOLVE_UIDS"), "yes")) 
             {
		passwdfile = fopen("/etc/passwd", "r");
		groupfile = fopen("/etc/group", "r");
	     } 

	setgid(userinfo.pw_gid);
	initgroups(userinfo.pw_name, userinfo.pw_gid);
	if (strcasecmp(config_getoption("DO_CHROOT"), "no")) {
		if (chroot(str)) {
			control_printf(SL_FAILURE, "421 Unable to change root directory.\r\n%s.",
					strerror(errno));
			exit(0);
		}
		if (bftpd_setuid(userinfo.pw_uid)) {
			control_printf(SL_FAILURE, "421 Unable to change uid.");
			exit(0);
		}
		if (chdir("/")) {
			control_printf(SL_FAILURE, "421 Unable to change working directory.\r\n%s.",
					 strerror(errno));
			exit(0);
		}
	} else {
		if (bftpd_setuid(userinfo.pw_uid)) {
			control_printf(SL_FAILURE, "421 Unable to change uid.");
			exit(0);
		}
		if (chdir(str)) {
			control_printf(SL_FAILURE, "230 Couldn't change cwd to '%s': %s.", str,
					 strerror(errno));
			chdir("/");
		}
	}

        }   /* end of if we are using regular authentication methods */

        else     /* we are using file authentication */
        {
            /* get home directory */
	    strcpy(str, config_getoption("ROOTDIR"));
            if (! str[0])
                strcpy(str, "%h");
	    replace(str, "%h", home_directory);
            replace(str, "%u", user);

            /* see if we should change root */
            if (! strcasecmp(config_getoption("DO_CHROOT"), "yes"))
            {
                if ( chroot(home_directory) )
                {
                    control_printf(SL_FAILURE, "421 Unable to change root directory.\r\n");
                    exit(0);
                }
                if ( chdir("/") )
                {
                    control_printf(SL_FAILURE, "421 Unable to change working directory.\r\n");
                    exit(0);
                }
            }
               
        }      /* end of using file auth */

        new_umask();
	print_file(230, config_getoption("MOTD_USER"));
	control_printf(SL_SUCCESS, "230 User logged in.");
#ifdef HAVE_UTMP_H
	bftpd_logwtmp(1);
#endif
        bftpdutmp_log(1);
	bftpd_log("Successfully logged in as user '%s'.\n", user);
        if (config_getoption("AUTO_CHDIR")[0])
            chdir(config_getoption("AUTO_CHDIR"));

	state = STATE_AUTHENTICATED;
	bftpd_cwd_init();

        /* a little clean up before we go*/
        if ( (home_directory) && ( strcmp(home_directory, "/" ) ) )
            free(home_directory);
	return 0;
}


/* Return 1 on failure and 0 on success. */
int checkpass(char *password)
{
    #ifndef NO_GETPWNAM
    if (!getpwnam(user))
		return 1;
    #endif

	if (!strcasecmp(config_getoption("ANONYMOUS_USER"), "yes"))
		return 0;

#ifdef WANT_PAM
	if (!strcasecmp(config_getoption("AUTH"), "pam"))
		return checkpass_pam(password);
	else
#endif
		return checkpass_pwd(password);
}



void login_end()
{
#ifdef WANT_PAM
	if (usepam)
		return end_pam();
#endif
#ifdef HAVE_UTMP_H
	wtmp_end();
#endif
}

int checkpass_pwd(char *password)
{
#ifdef HAVE_SHADOW_H
	struct spwd *shd;
#endif
	if (strcmp(userinfo.pw_passwd, (char *) crypt(password, userinfo.pw_passwd))) {
#ifdef HAVE_SHADOW_H
		if (!(shd = getspnam(user)))
			return 1;
		if (strcmp(shd->sp_pwdp, (char *) crypt(password, shd->sp_pwdp)))
#endif
			return 1;
	}
	return 0;
}

#ifdef WANT_PAM
int conv_func(int num_msg, const struct pam_message **msgm,
			  struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *response;
	int i;
	response = (struct pam_response *) malloc(sizeof(struct pam_response) * num_msg);
	for (i = 0; i < num_msg; i++) {
		response[i].resp = (char *) strdup(appdata_ptr);
		response[i].resp_retcode = 0;
	}
	*resp = response;
	return 0;
}

int checkpass_pam(char *password)
{
	struct pam_conv conv = { conv_func, password };
	int retval = pam_start("bftpd", user, (struct pam_conv *) &conv,
						   (pam_handle_t **) & pamh);
	if (retval != PAM_SUCCESS) {
		printf("Error while initializing PAM: %s\n",
			   pam_strerror(pamh, retval));
		return 1;
	}
        /* 
        Allow Bftpd to build with OpenPAM
	pam_fail_delay(pamh, 0);
        */
	retval = pam_authenticate(pamh, 0);
	if (retval == PAM_SUCCESS)
		retval = pam_acct_mgmt(pamh, 0);
	if (retval == PAM_SUCCESS)
		pam_open_session(pamh, 0);
	if (retval != PAM_SUCCESS)
		return 1;
	else
		return 0;
}

void end_pam()
{
	if (pamh) {
		pam_close_session(pamh, 0);
		pam_end(pamh, 0);
	}
}
#endif

int checkuser()
{

	FILE *fd;
	char *p;
	char line[256];

	if ((fd = fopen(config_getoption("PATH_FTPUSERS"), "r"))) {
		while (fgets(line, sizeof(line), fd))
			if ((p = strchr(line, '\n'))) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				if (!strcasecmp(line, user)) {
					fclose(fd);
					return 1;
				}
			}
		fclose(fd);
	}
	return 0;
}

int checkshell()
{
#ifdef HAVE_GETUSERSHELL
	char *cp;
	struct passwd *pwd;

    if (!strcasecmp(config_getoption("AUTH_ETCSHELLS"), "no"))
        return 0;
    
	pwd = getpwnam(user);
	while ((cp = getusershell()))
		if (!strcmp(cp, pwd->pw_shell))
			break;
	endusershell();

	if (!cp)
		return 1;
	else
		return 0;
#else
    return 0;
#   warning "Your system doesn't have getusershell(). You can not"
#   warning "use /etc/shells authentication with bftpd."
#endif
}




/*
This function searches through a text file for a matching
username. If a match is found, the password in the
text file is compared to the password passed in to
the function. If the password matches, the function
returns the fourth field (home directory). On failure,
it returns NULL.
-- Jesse
*/
char *check_file_password(char *my_filename, char *my_username, char *my_password)
{
   FILE *my_file;
   int found_user = 0;
   char user[33], password[33], group[33], home_dir[33];
   char *my_home_dir = NULL;
   int return_value;

   my_file = fopen(my_filename, "r");
   if (! my_file)
      return NULL;

   return_value = fscanf(my_file, "%32s %32s %32s %32s", user, password, group, home_dir);
   if (! strcmp(user, my_username) )
      found_user = 1;

   while ( (! found_user) && ( return_value != EOF) )
   {
       return_value = fscanf(my_file, "%32s %32s %32s %32s", user, password, group, home_dir);
       if (! strcmp(user, my_username) )
          found_user = 1;
   }

   fclose(my_file);
   if (found_user)
   {
      /* check password */
      if (! strcmp(password, "*") )
      {
      }
      else if ( strcmp(password, my_password) )
         return NULL;

      my_home_dir = calloc( strlen(home_dir) + 1, sizeof(char) );
      if (! my_home_dir)
          return NULL;
      strcpy(my_home_dir, home_dir);
   }
  
   return my_home_dir;
}

