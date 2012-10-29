#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
/* added for FreeBSD compatibility */
#include <limits.h>
#include <signal.h>

#define __USE_GNU
#include <unistd.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_ASM_SOCKET_H
#include <asm/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_WAIT_H
# include <wait.h>
#else
# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif

#include "mystring.h"
#include "login.h"
#include "logging.h"
#include "dirlist.h"
#include "options.h"
#include "main.h"
#include "targzip.h"
#include "cwd.h"
#include "bftpdutmp.h"

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#else
#undef WANT_GZIP
#endif

int state = STATE_CONNECTED;
char user[USERLEN + 1];
struct sockaddr_in sa;
char pasv = 0;
int sock;
int pasvsock;
char *philename = NULL;
unsigned long offset = 0;
short int xfertype = TYPE_BINARY;
int ratio_send = 1, ratio_recv = 1;
long unsigned bytes_sent = 0, bytes_recvd = 0;
int epsvall = 0;
int xfer_bufsize;


void control_printf(char success, char *format, ...)
{
    char buffer[256];
    va_list val;
    va_start(val, format);
    vsnprintf(buffer, sizeof(buffer), format, val);
    va_end(val);
    fprintf(stderr, "%s\r\n", buffer);
    replace(buffer, "\r", "");
    bftpd_statuslog(3, success, "%s", buffer);
}

void new_umask()
{
    int um;
    unsigned long get_um;
    char *foo = config_getoption("UMASK");
    if (!foo[0])
        um = 022;
    else
    {
        get_um = strtoul(foo, NULL, 8);
        if (get_um <= INT_MAX)
           um = get_um;
        else
        {
           bftpd_log("Error with umask value. Setting to 022.\n", 0);
           um = 022;
        }
    }
    umask(um);
}

void prepare_sock(int sock)
{
    int on = 1;
#ifdef TCP_NODELAY
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &on, sizeof(on));
#endif
#ifdef TCP_NOPUSH
	setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, (void *) &on, sizeof(on));
#endif
#ifdef SO_REUSEADDR
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on));
#endif
#ifdef SO_REUSEPORT
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void *) &on, sizeof(on));
#endif
#ifdef SO_SNDBUF
	on = 65536;
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *) &on, sizeof(on));
#endif
}

int dataconn()
{
	struct sockaddr foo;
	struct sockaddr_in local;
	socklen_t namelen = sizeof(foo);
    int curuid = geteuid();

	memset(&foo, 0, sizeof(foo));
	memset(&local, 0, sizeof(local));

	if (pasv) {
		sock = accept(pasvsock, (struct sockaddr *) &foo, (socklen_t *) &namelen);
		if (sock == -1) {
            control_printf(SL_FAILURE, "425-Unable to accept data connection.\r\n425 %s.",
                     strerror(errno));
			return 1;
		}
        close(pasvsock);
        prepare_sock(sock);
	} else {
		sock = socket(AF_INET, SOCK_STREAM, 0);
        prepare_sock(sock);
		local.sin_addr.s_addr = name.sin_addr.s_addr;
		local.sin_family = AF_INET;
        if (!strcasecmp(config_getoption("DATAPORT20"), "yes")) {
            seteuid(0);
            local.sin_port = htons(20);
        }
		if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
			control_printf(SL_FAILURE, "425-Unable to bind data socket.\r\n425 %s.",
					strerror(errno));
			return 1;
		}
        if (!strcasecmp(config_getoption("DATAPORT20"), "yes"))
            seteuid(curuid);
		sa.sin_family = AF_INET;
		if (connect(sock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
			control_printf(SL_FAILURE, "425-Unable to establish data connection.\r\n"
                    "425 %s.", strerror(errno));
			return 1;
		}
	}
	control_printf(SL_SUCCESS, "150 %s data connection established.",
	               xfertype == TYPE_BINARY ? "BINARY" : "ASCII");
	return 0;
}

void init_userinfo()
{
    #ifndef NO_GETPWNAM
    struct passwd *temp = getpwnam(user);
    if (temp) {
        userinfo.pw_name = strdup(temp->pw_name);
        userinfo.pw_passwd = strdup(temp->pw_passwd);
        userinfo.pw_uid = temp->pw_uid;
        userinfo.pw_gid = temp->pw_gid;
        userinfo.pw_gecos = strdup(temp->pw_gecos);
        userinfo.pw_dir = strdup(temp->pw_dir);
        userinfo.pw_shell = strdup(temp->pw_shell);
        userinfo_set = 1;
    }
    #endif
}

void command_user(char *username)
{
	char *alias;
	if (state) {
		control_printf(SL_FAILURE, "503 Username already given.");
		return;
	}
	mystrncpy(user, username, sizeof(user) - 1);
    userinfo_set = 1; /* Dirty! */
	alias = (char *) config_getoption("ALIAS");
    userinfo_set = 0;
	if (alias[0] != '\0')
		mystrncpy(user, alias, sizeof(user) - 1);
    init_userinfo();
#ifdef DEBUG
	bftpd_log("Trying to log in as %s.\n", user);
#endif
    expand_groups();
	if (!strcasecmp(config_getoption("ANONYMOUS_USER"), "yes"))
		bftpd_login("");
	else {
		state = STATE_USER;
		control_printf(SL_SUCCESS, "331 Password please.");
	}
}

void command_pass(char *password)
{
	if (state > STATE_USER) {
		control_printf(SL_FAILURE, "503 Already logged in.");
		return;
	}
	if (bftpd_login(password)) {
		bftpd_log("Login as user '%s' failed.\n", user);
		control_printf(SL_FAILURE, "421 Login incorrect.");
		exit(0);
	}
}

void command_pwd(char *params)
{
     char *my_cwd = NULL;

     my_cwd = bftpd_cwd_getcwd();
     if (my_cwd)
     {
         control_printf(SL_SUCCESS, "257 \"%s\" is the current working directory.", my_cwd);
         free(my_cwd);
     }
}


void command_type(char *params)
{
    if ((*params == 'A') || (*params == 'a')) {
        control_printf(SL_SUCCESS, "200 Transfer type changed to ASCII");
        xfertype = TYPE_ASCII;
    } else if ((*params == 'I') || (*params == 'i')) {
      	control_printf(SL_SUCCESS, "200 Transfer type changed to BINARY");
        xfertype = TYPE_BINARY;
    } else
        control_printf(SL_FAILURE, "500 Type '%c' not supported.", *params);
}

void command_port(char *params) {
  unsigned long a0, a1, a2, a3, p0, p1, addr;
  if (epsvall) {
      control_printf(SL_FAILURE, "500 EPSV ALL has been called.");
      return;
  }
  sscanf(params, "%lu,%lu,%lu,%lu,%lu,%lu", &a0, &a1, &a2, &a3, &p0, &p1);
  addr = htonl((a0 << 24) + (a1 << 16) + (a2 << 8) + a3);
  if((addr != remotename.sin_addr.s_addr) &&( strncasecmp(config_getoption("ALLOW_FXP"), "yes", 3))) {
      control_printf(SL_FAILURE, "500 The given address is not yours.");
      return;
  }
  sa.sin_addr.s_addr = addr;
  sa.sin_port = htons((p0 << 8) + p1);
  if (pasv) {
    close(sock);
    pasv = 0;
  }
  control_printf(SL_SUCCESS, "200 PORT %lu.%lu.%lu.%lu:%lu OK",
           a0, a1, a2, a3, (p0 << 8) + p1);
}

void command_eprt(char *params) {
    char delim;
    int af;
    char addr[51];
    char foo[20];
    int port;
    if (epsvall) {
        control_printf(SL_FAILURE, "500 EPSV ALL has been called.");
        return;
    }
    if (strlen(params) < 5) {
        control_printf(SL_FAILURE, "500 Syntax error.");
        return;
    }
    delim = params[0];
    sprintf(foo, "%c%%i%c%%50[^%c]%c%%i%c", delim, delim, delim, delim, delim);
    if (sscanf(params, foo, &af, addr, &port) < 3) {
        control_printf(SL_FAILURE, "500 Syntax error.");
        return;
    }
    if (af != 1) {
        control_printf(SL_FAILURE, "522 Protocol unsupported, use (1)");
        return;
    }
    sa.sin_addr.s_addr = inet_addr(addr);
    if ((sa.sin_addr.s_addr != remotename.sin_addr.s_addr) && (strncasecmp(config_getoption("ALLOW_FXP"), "yes", 3))) {
        control_printf(SL_FAILURE, "500 The given address is not yours.");
        return;
    }
    sa.sin_port = htons(port);
    if (pasv) {
        close(sock);
        pasv = 0;
    }
    control_printf(SL_FAILURE, "200 EPRT %s:%i OK", addr, port);
}

void command_pasv(char *foo)
{
	int a1, a2, a3, a4;
        socklen_t namelen;
	struct sockaddr_in localsock;
        char *my_override_ip;

    if (epsvall) {
        control_printf(SL_FAILURE, "500 EPSV ALL has been called.");
        return;
    }
	pasvsock = socket(AF_INET, SOCK_STREAM, 0);
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_family = AF_INET;

    if (!config_getoption("PASSIVE_PORTS") || !strlen(config_getoption("PASSIVE_PORTS"))) {
        /* bind to any port */
        sa.sin_port = 0;
        if (bind(pasvsock, (struct sockaddr *) &sa, sizeof(sa)) == -1) 
        {
            control_printf(SL_FAILURE, "425-Error: Unable to bind data socket.\r\n425 %s", strerror(errno));
            return;
        }
    } 

     else {
        int i = 0, success = 0, port;
        for (;;) {
            port = int_from_list(config_getoption("PASSIVE_PORTS"), i++);
            if (port < 0)
                break;
            sa.sin_port = htons(port);
            if (bind(pasvsock, (struct sockaddr *) &sa, sizeof(sa)) == 0) {
                success = 1;
#ifdef DEBUG
                bftpd_log("Passive mode: Successfully bound port %d\n", port);
#endif
                break;
            }
        }   /* end of for loop */
        if (!success) {
            control_printf(SL_FAILURE, "425 Error: Unable to bind data socket.");
            return;
        }
        prepare_sock(pasvsock);
    }    /* end of else using list of ports */
      
	if (listen(pasvsock, 1)) {
		control_printf(SL_FAILURE, "425-Error: Unable to make socket listen.\r\n425 %s",
				 strerror(errno));
		return;
	}
	namelen = sizeof(localsock);
	getsockname(pasvsock, (struct sockaddr *) &localsock, (socklen_t *) &namelen);

        /* see if we should over-ride the IP address sent to the client */
        my_override_ip = config_getoption("OVERRIDE_IP");
        if (my_override_ip[0])
        {
            sscanf( my_override_ip, "%i.%i.%i.%i",
                    &a1, &a2, &a3, &a4);
        }
        else   /* noraml, no over-ride */
        {
  	    sscanf((char *) inet_ntoa(name.sin_addr), "%i.%i.%i.%i",
		   &a1, &a2, &a3, &a4);
        }

	control_printf(SL_SUCCESS, "227 Entering Passive Mode (%i,%i,%i,%i,%i,%i)", a1, a2, a3, a4,
			 ntohs(localsock.sin_port) >> 8, ntohs(localsock.sin_port) & 0xFF);
	pasv = 1;
}

void command_epsv(char *params)
{
    struct sockaddr_in localsock;
    socklen_t namelen;
    int af;
    if (params[0]) {
        if (!strncasecmp(params, "ALL", 3))
            epsvall = 1;
        else {
            if (sscanf(params, "%i", &af) < 1) {
                control_printf(SL_FAILURE, "500 Syntax error.");
                return;
            } else {
                if (af != 1) {
                    control_printf(SL_FAILURE, "522 Protocol unsupported, use (1)");
                    return;
                }
            }
        }
    }
    pasvsock = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = 0;
    sa.sin_family = AF_INET;
    if (bind(pasvsock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
		control_printf(SL_FAILURE, "500-Error: Unable to bind data socket.\r\n425 %s",
				 strerror(errno));
		return;
	}
	if (listen(pasvsock, 1)) {
		control_printf(SL_FAILURE, "500-Error: Unable to make socket listen.\r\n425 %s",
				 strerror(errno));
		return;
	}
	namelen = sizeof(localsock);
	getsockname(pasvsock, (struct sockaddr *) &localsock, (socklen_t *) &namelen);
    control_printf(SL_SUCCESS, "229 Entering extended passive mode (|||%i|)",
             ntohs(localsock.sin_port));
    pasv = 1;
}

char test_abort(char selectbefore, int file, int sock)
{
    char str[256];
    fd_set rfds;
    struct timeval tv;
    if (selectbefore) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(fileno(stdin), &rfds);
        if (!select(fileno(stdin) + 1, &rfds, NULL, NULL, &tv))
            return 0;
    }
	fgets(str, sizeof(str), stdin);
    if (strstr(str, "ABOR")) {
        control_printf(SL_SUCCESS, "426 Transfer aborted.");
    	close(file);
		close(sock);
   		control_printf(SL_SUCCESS, "226 Aborted.");
		bftpd_log("Client aborted file transmission.\n");
        alarm(control_timeout);
        return 1;
	}
    return 0;
}

void command_allo(char *foo)
{
    command_noop(foo);
}


/* This function allows the storage of multiple files on the server. */
void command_mput(char *filenames)
{
   char filename[MAXCMD];  /* single filename */
   int from_index, to_index;      /* position in "filenames" and "filename" */
                                                                                                                             
   from_index = 0;     /* start at begining of filenames */
   memset(filename, 0, MAXCMD);       /* clear filename */
   to_index = 0;

   /* go until we find a NULL character */
   while ( filenames[from_index] > 0)
   {
       /* copy filename until we hit a space */
       if (filenames[from_index] == ' ')
       {
          /* got a full filename */
          command_stor(filename);
          /* clear filename and reset to_index */
          to_index = 0;
          memset(filename, 0, MAXCMD);

          while (filenames[from_index] == ' ')
            from_index++;    /* goto next position */
       }
                                                                                                                             
       /* if we haven't hit a space, then copy the letter */
       else
       {
          filename[to_index] = filenames[from_index];
          to_index++;
          from_index++;
          /* if the next character is a NULL, then this is the end of the filename */
          if (! filenames[from_index])
          {
             command_stor(filename);   /* get the file */
             to_index = 0;             /* reset filename index */
             memset(filename, 0, MAXCMD);    /* clear filename buffer */
             from_index++;                /* goto next character */
          }
       }
                                                                                                                             
       /* if the buffer is getting too big, then stop */
       if (to_index > (MAXCMD - 2) )
       {
          bftpd_log("Error: Filename in '%s' too long.\n", filenames);
          return;
       }
                                                                                                                             
    }   /* end of while */
                                                                                                                             
}



void do_stor(char *filename, int flags)
{
    char *buffer;
    int fd, i, max;
    fd_set rfds;
    struct timeval tv;
    char *p, *pp;
    char *mapped = bftpd_cwd_mappath(filename);

    int my_buffer_size;    /* total transfer buffer size divided by number of clients */
    int num_clients;       /* number of clients connected to the server */
    int new_num_clients;
    int xfer_delay;
    int attempt_gzip = FALSE;
    unsigned long get_value;
    #ifdef HAVE_ZLIB_H
    gzFile my_zip_file = NULL;
    #endif

    if (pre_write_script)
       run_script(pre_write_script, mapped);

    #ifdef HAVE_ZLIB_H
    if (! strcmp( config_getoption("GZ_UPLOAD"), "yes") )
    {
       attempt_gzip = TRUE;
       strcat(mapped, ".gz");
    }
    else
       attempt_gzip = FALSE;
    #endif
 
        /* See if we should delay between data transfers */
        get_value = strtoul( config_getoption("XFER_DELAY"), NULL, 0);
        if (get_value <= INT_MAX)
           xfer_delay = get_value;
        else
        {
           bftpd_log("Error getting xfer_delay in do_stor().\n", 0);
           xfer_delay = 0;
        }

        /* Check to see if the file exists and if we can over-write
           it, if it does. -- Jesse */
        fd = open(mapped, O_RDONLY);
        if (fd >= 0)  /* file exists */
        {
           /* close the file */
           close(fd);
           /* check if we can over-write it */
           if ( !strcasecmp( config_getoption("ALLOWCOMMAND_DELE"), "no") )
           {
              bftpd_log("Not allowed to over-write '%s'.\n", filename);
              control_printf(SL_FAILURE, 
                     "553 Error: Remote file is write protected.");

              free(mapped);
              close(sock);
              return;
           }
        }

        if (! attempt_gzip)
        {        
	  fd = open(mapped, flags, 00666);
	  /*
             do this below
             if (mapped)
	 	free(mapped);
          */
	  if (fd == -1) {
		bftpd_log("Error: '%s' while trying to store file '%s'.\n",
				  strerror(errno), filename);
		control_printf(SL_FAILURE, "553 Error: %s.", strerror(errno));

                close(fd);     /* make sure it is not open */
                if (post_write_script)
                   run_script(post_write_script, mapped);
                free(mapped);
		return;
          }
	}

        #ifdef HAVE_ZLIB_H
        if ( attempt_gzip )
        {
           my_zip_file = gzopen(mapped, "wb+");
           if (mapped)
           {
               free(mapped);
               mapped = NULL;
           }
           if (! my_zip_file)
           {
              control_printf(SL_FAILURE, "553 Error: An error occured creating compressed file.");
              close(sock);
              close(fd);
              return;
           }
        }
        #endif

	bftpd_log("Client is storing file '%s'.\n", filename);
	if (dataconn())
        {
            close(fd);
            if (post_write_script)
               run_script(post_write_script, mapped);
            if (mapped)
               free(mapped);
	    return;
        }

    /* Figure out how big the transfer buffer should be.
       This will be the total size divided by the number of clients connected.
       -- Jesse
    */
    num_clients = bftpdutmp_usercount("*");
    my_buffer_size = get_buffer_size(num_clients);

    alarm(0);
    buffer = malloc(xfer_bufsize);
    /* Check to see if we are out of memory. -- Jesse */
    if (! buffer)
    {
       bftpd_log("Unable to create buffer to receive file.\n", 0);
       control_printf(SL_FAILURE, "553 Error: An unknown error occured on the server.");
       if (fd >= 0)
          close(fd);
       close(sock);
       return;
    }

	lseek(fd, offset, SEEK_SET);
	offset = 0;
    /* Do not use the whole buffer, because a null byte has to be
     * written after the string in ASCII mode. */
    max = (sock > fileno(stdin) ? sock : fileno(stdin)) + 1;
	for (;;) {
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        FD_SET(fileno(stdin), &rfds);
        
        tv.tv_sec = data_timeout;
        tv.tv_usec = 0;
        if (!select(max, &rfds, NULL, NULL, &tv)) {
            close(sock);
            close(fd);
            control_printf(SL_FAILURE, "426 Kicked due to data transmission timeout.");
            bftpd_log("Kicked due to data transmission timeout.\n");
            /* Before we exit, let's remove our entry in the log file. -- Jesse */
            if (post_write_script)
               run_script(post_write_script, mapped);

            bftpdutmp_end();
            exit(0);
        }
        if (FD_ISSET(fileno(stdin), &rfds)) {
            test_abort(0, fd, sock);
	    if (buffer)
		free(buffer);
            close(fd);
            if (post_write_script)
               run_script(post_write_script, mapped);
            free(mapped);
            return;
        }

	if (!((i = recv(sock, buffer, my_buffer_size - 1, 0))))
            break;
        bytes_recvd += i;
        if (xfertype == TYPE_ASCII) {
            buffer[i] = '\0';
            /* on ASCII stransfer, strip character 13 */
            p = pp = buffer;
    	    while (*p) {
        		if ((unsigned char) *p == 13)
        			p++;
        		else
        			*pp++ = *p++;
            }   
        	*pp++ = 0;
            i = strlen(buffer);
        }     // end of if ASCII type transfer

        #ifdef HAVE_ZLIB_H
           if (my_zip_file)
               gzwrite( my_zip_file, buffer, i );    
        #endif
           if(! attempt_gzip)
              write(fd, buffer, i);

        /* Check to see if our bandwidth usage should change. -- Jesse */
        new_num_clients = bftpdutmp_usercount("*");
        if (new_num_clients != num_clients)
        {
            num_clients = new_num_clients;
            my_buffer_size = get_buffer_size(num_clients);
        }

        /* check for transfer delay */
        if ( xfer_delay )
        {
            struct timeval wait_time;

            wait_time.tv_sec = 0;
            wait_time.tv_usec = xfer_delay;
            select( 0, NULL, NULL, NULL, &wait_time);
        }


	}     // end of for loop, reading

	free(buffer);
        #ifdef HAVE_ZLIB_H
          gzclose(my_zip_file);
        #else
	  close(fd);
        #endif

	close(sock);
        alarm(control_timeout);
        offset = 0;
	control_printf(SL_SUCCESS, "226 File transmission successful.");
	bftpd_log("File transmission successful.\n");
        if (post_write_script)
          run_script(post_write_script, mapped);

        if (mapped)
           free(mapped);
}

void command_stor(char *filename)
{
    do_stor(filename, O_CREAT | O_WRONLY | O_TRUNC);
}

void command_appe(char *filename)
{
    do_stor(filename, O_CREAT | O_WRONLY | O_APPEND);
}




/* Send multpile files to the client. */
void command_mget(char *filenames)
{
   char filename[MAXCMD];  /* single filename */
   int from_index, to_index;      /* position in "filenames" and "filename" */

   from_index = 0;     /* start at begining of filenames */
   memset(filename, 0, MAXCMD);       /* clear filename */
   to_index = 0;   

   /* go until we find a NULL character */
   while ( filenames[from_index] > 0)
   {
       /* copy filename until we hit a space */
       if (filenames[from_index] == ' ') 
       {
          /* got a full filename */
          command_retr(filename);
          /* clear filename and reset to_index */
          to_index = 0;
          memset(filename, 0, MAXCMD);

          while (filenames[from_index] == ' ')
            from_index++;    /* goto next position */
       }
       
       /* if we haven't hit a space, then copy the letter */
       else
       {
          filename[to_index] = filenames[from_index];
          to_index++;
          from_index++;
          /* if the next character is a NULL, then this is the end of the filename */
          if (! filenames[from_index])
          {
             command_retr(filename);   /* send the file */
             to_index = 0;             /* reset filename index */
             memset(filename, 0, MAXCMD);    /* clear filename buffer */
             from_index++;                /* goto next character */
          }
       }

       /* if the buffer is getting too big, then stop */
       if (to_index > (MAXCMD - 2) )
       {
	  bftpd_log("Error: Filename in '%s' too long.\n", filenames);
          return;
       }

    }   /* end of while */
   
}

void command_retr(char *filename)
{
        int num_clients, new_num_clients;   /* number of connectiosn to the server */
        int my_buffer_size;                 /* size of the transfer buffer to use */
	char *mapped = NULL;
	char *buffer;
        int xfer_delay;
        struct timeval wait_time;
        unsigned long get_value;
        ssize_t send_status;

#if (defined(WANT_GZIP) || defined(HAVE_ZLIB_H))
    gzFile gzfile;
#endif
	int phile;
	int i, whattodo = DO_NORMAL;
	struct stat statbuf;
#if (defined(WANT_TAR) && defined(WANT_GZIP))
    int filedes[2];
#endif
#if (defined(WANT_TAR) || defined(WANT_GZIP))
    char *foo;
#endif
#ifdef WANT_TAR
    char *argv[4];
#endif

        get_value = strtoul( config_getoption("XFER_DELAY"), NULL, 0);
        if (get_value <= INT_MAX)
           xfer_delay = get_value;
        else
        {
           bftpd_log("Error getting XFER_DELAY in command_retr().\n", 0);
           xfer_delay = 0;
        }

	mapped = bftpd_cwd_mappath(filename);
        if (! mapped)
        {
           bftpd_log("Memory error in sending file.\n", 0);
           control_printf(SL_FAILURE, "553 An unknown error occured on the server.", 9);
           return;
        }

	phile = open(mapped, O_RDONLY);
	if (phile == -1) {       // failed to open a file
#if (defined(WANT_TAR) && defined(WANT_GZIP))
		if ((foo = strstr(filename, ".tar.gz")))
			if (strlen(foo) == 7) {
				whattodo = DO_TARGZ;
				*foo = '\0';
			}
#endif
#ifdef WANT_TAR
		if ((foo = strstr(filename, ".tar")))
			if (strlen(foo) == 4) {
				whattodo = DO_TARONLY;
				*foo = '\0';
			}
#endif
#ifdef WANT_GZIP
		if ((foo = strstr(filename, ".gz")))
			if (strlen(foo) == 3) {
				whattodo = DO_GZONLY;
				*foo = '\0';
			}
#endif
		if (whattodo == DO_NORMAL) {
			bftpd_log("Error: '%s' while trying to receive file '%s'.\n",
					  strerror(errno), filename);
			control_printf(SL_FAILURE, "553 Error: %s.", strerror(errno));
			if (mapped)
				free(mapped);
			return;
		}
	}

        #ifdef HAVE_ZLIB_H
        else  // we did open a file
        {
            char *my_temp;
            char *zip_option;

            my_temp = strstr(filename, ".gz");
            zip_option = config_getoption("GZ_DOWNLOAD");
            if (my_temp)
            {
               if ( ( strlen(my_temp) == 3) && (! strcasecmp(zip_option, "yes") ) )
                  whattodo = DO_GZUNZIP;
            }
        }
        #endif

	stat(mapped, (struct stat *) &statbuf);
	if (S_ISDIR(statbuf.st_mode)) {
		control_printf(SL_FAILURE, "550 Error: Is a directory.");
		if (mapped)
			free(mapped);
		return;
	}

	if ((((statbuf.st_size - offset) * ratio_send) / ratio_recv > bytes_recvd
		 - bytes_sent) && (strcmp((char *) config_getoption("RATIO"), "none"))) {
		bftpd_log("Error: 'File too big (ratio)' while trying to receive file "
				  "'%s'.\n", filename);
		control_printf(SL_FAILURE, "553 File too big. Send at least %i bytes first.",
				(int) (((statbuf.st_size - offset) * ratio_send) / ratio_recv)
				- bytes_recvd);
		if (mapped)
			free(mapped);
		return;
	}
	bftpd_log("Client is receiving file '%s'.\n", filename);
	switch (whattodo) {
#if (defined(WANT_TAR) && defined(WANT_GZIP))
        case DO_TARGZ:
            close(phile);
            if (dataconn()) {
				if (mapped)
					free(mapped);
                return;
			}
            alarm(0);
            pipe(filedes);
            if (fork()) {
                buffer = malloc(xfer_bufsize);
                /* check to make sure alloc worked */
                if (! buffer)
                {
                   if (mapped)
                      free(mapped);
                   bftpd_log("Memory error in sending file.\n", 0);
                   control_printf(SL_FAILURE, "553 An unknown error occured on the server.", 9);
                   return;
                }

                /* find the size of the transfer buffer divided by number of connections */
                num_clients =  bftpdutmp_usercount("*");
                my_buffer_size = get_buffer_size(num_clients);
                
                close(filedes[1]);
                gzfile = gzdopen(sock, "wb");
                while ((i = read(filedes[0], buffer, my_buffer_size))) {
                    gzwrite(gzfile, buffer, i);
                    test_abort(1, phile, sock);

                    /* check for a change in number of connections */
                    new_num_clients = bftpdutmp_usercount("*");
                    if (new_num_clients != num_clients)
                    {
                        num_clients = new_num_clients;
                        my_buffer_size = get_buffer_size(num_clients);
                    }
                    /* pause between transfers */
                    if (xfer_delay)
                    {
                        wait_time.tv_sec = 0;
                        wait_time.tv_usec = xfer_delay;
                        select( 0, NULL, NULL, NULL, &wait_time);
                    }
                }     // end of while 
                free(buffer);
                gzclose(gzfile);
                wait(NULL); /* Kill the zombie */
            } else {
                stderr = devnull;
                close(filedes[0]);
                close(fileno(stdout));
                dup2(filedes[1], fileno(stdout));
                setvbuf(stdout, NULL, _IONBF, 0);
                argv[0] = "tar";
                argv[1] = "cf";
                argv[2] = "-";
                argv[3] = mapped;
                exit(pax_main(4, argv));
            }
            break;
#endif
#ifdef WANT_TAR
		case DO_TARONLY:
            if (dataconn()) {
				if (mapped)
					free(mapped);
                return;
			}
            alarm(0);
            if (fork())
                wait(NULL);
            else {
                stderr = devnull;
                dup2(sock, fileno(stdout));
                argv[0] = "tar";
                argv[1] = "cf";
                argv[2] = "-";
                argv[3] = mapped;
                exit(pax_main(4, argv));
            }
            break;
#endif
#ifdef WANT_GZIP
		case DO_GZONLY:
			if (mapped)
                        {
				free(mapped);
                                mapped = NULL;
                        }
            if ((phile = open(mapped, O_RDONLY)) < 0) {
				control_printf(SL_FAILURE, "553 Error: %s.", strerror(errno));
				return;
			}
			if (dataconn()) {
				if (mapped)
					free(mapped);
				return;
			}
            alarm(0);
            buffer = malloc(xfer_bufsize);
            /* check for alloc error */
            if (! buffer)
            {
               bftpd_log("Memory error while sending file.", 0);
               control_printf(SL_FAILURE, "553 An unknown error occured on the server.", 0);
               if (phile) close(phile);
               return;
            }

            /* check buffer size based on number of connections */
            num_clients = bftpdutmp_usercount("*");
            my_buffer_size = get_buffer_size(num_clients);
            /* Use "wb9" for maximum compression, uses more CPU time... */
            gzfile = gzdopen(sock, "wb");
            while ((i = read(phile, buffer, my_buffer_size))) {
                gzwrite(gzfile, buffer, i);
                test_abort(1, phile, sock);
                new_num_clients = bftpdutmp_usercount("*");
                if ( new_num_clients != num_clients )
                {
                    num_clients = new_num_clients;
                    my_buffer_size = get_buffer_size(num_clients);
                }
                /* pause between transfers */
                if (xfer_delay)
                {
                    wait_time.tv_sec = 0;
                    wait_time.tv_usec = xfer_delay;
                    select( 0, NULL, NULL, NULL, &wait_time);
                }
            }
            free(buffer);
            close(phile);
            gzclose(gzfile);
            break;
#endif

#ifdef HAVE_ZLIB_H
            case DO_GZUNZIP:
               if ( dataconn() )
                  return;

               gzfile = gzdopen(phile, "rb");
               if (! gzfile)
               {
                   close(phile);
                   bftpd_log("Memory error while sending file.", 0);
                   control_printf(SL_FAILURE, "553 An unknown error occured on the server.", 0);
                   return;
               }

               alarm(0);
               buffer = malloc(xfer_bufsize);
               if (! buffer)
               {
                  close(phile);
                  gzclose(gzfile);
                  bftpd_log("Memory error while sending file.", 0);
                  control_printf(SL_FAILURE, "553 An unknown error occured on the server.", 0);
                  return;
               }

               /* check buffer size based on number of connections */
               num_clients = bftpdutmp_usercount("*");
               my_buffer_size = get_buffer_size(num_clients);

               i = gzread(gzfile, buffer, my_buffer_size);
               while ( i )
               {
                   write(sock, buffer, i);
                   // test_abort(1, phile, sock);

                   new_num_clients = bftpdutmp_usercount("*");
                   if ( new_num_clients != num_clients )
                   {
                      num_clients = new_num_clients;
                      my_buffer_size = get_buffer_size(num_clients);
                   }
                   /* pause between transfers */
                   if (xfer_delay)
                   {
                      wait_time.tv_sec = 0;
                      wait_time.tv_usec = xfer_delay;
                      select( 0, NULL, NULL, NULL, &wait_time);
                   }

                   i = gzread(gzfile, buffer, my_buffer_size);
               }   // end of while not end of file

               free(buffer);
               close(phile);
               gzclose(gzfile);
            break;    // send file and unzip on the fly
#endif

		case DO_NORMAL:
                       /* used to be commented out */
			if (mapped)
                        {
				free(mapped);
                                mapped = NULL;
                        }
			if (dataconn())
				return;
            alarm(0);
			lseek(phile, offset, SEEK_SET);
			offset = 0;
			buffer = malloc(xfer_bufsize * 2 + 1);
                        /* make sure buffer was created */
                        if (! buffer)
                        {
                            control_printf(SL_FAILURE, "553 An unknown error occured.");
                            bftpd_log("Memory error while trying to send file.", 0);
                            close(sock);
                            close(phile);
                            return;
                        }
                        num_clients = bftpdutmp_usercount("*");
                        my_buffer_size = get_buffer_size(num_clients);
			while ((i = read(phile, buffer, my_buffer_size))) {
				if (test_abort(1, phile, sock)) {
					free(buffer);
					return;
				}

                if (xfertype == TYPE_ASCII) {
                    buffer[i] = '\0';
                    i += replace(buffer, "\n", "\r\n");
                }
				send_status = send(sock, buffer, i, 0);
                                // check for dropped connection
                                if (send_status < 0)
                                {
                                   free(buffer);
                                   close(phile);
                                   close(sock);
                                   alarm(control_timeout);
                                   control_printf(SL_SUCCESS, "426 Transfer aborted.");
                                   control_printf(SL_SUCCESS, "226 Aborted.");
                                   bftpd_log("File transmission interrupted. Send failed.\n");
                                   return;
                                }

				bytes_sent += i;
              
                                new_num_clients = bftpdutmp_usercount("*");
                                my_buffer_size = get_buffer_size(num_clients);
                                /* pause between transfers */
                                if (xfer_delay)
                                {
                                   wait_time.tv_sec = 0;
                                   wait_time.tv_usec = xfer_delay;
                                   select( 0, NULL, NULL, NULL, &wait_time);
                                }

			}       // end of while
            free(buffer);
            }

	close(phile);
	close(sock);
        offset = 0;
        alarm(control_timeout);
	control_printf(SL_SUCCESS, "226 File transmission successful.");
	bftpd_log("File transmission of '%s' successful.\n", filename);
        if (mapped) free(mapped);
}

void do_dirlist(char *dirname, char verbose)
{
	FILE *datastream;
	if (dirname[0] != '\0') {
		/* skip arguments */
		if (dirname[0] == '-') {
			while ((dirname[0] != ' ') && (dirname[0] != '\0'))
				dirname++;
			if (dirname[0] != '\0')
				dirname++;
		}
	}
	if (dataconn())
		return;
    alarm(0);
	datastream = fdopen(sock, "w");
	if (dirname[0] == '\0')
		dirlist("*", datastream, verbose);
	else {
		char *mapped = bftpd_cwd_mappath(dirname);
		dirlist(mapped, datastream, verbose);
		free(mapped);
	}
	fclose(datastream);
    alarm(control_timeout);
	control_printf(SL_SUCCESS, "226 Directory list has been submitted.");
}

void command_list(char *dirname)
{
	do_dirlist(dirname, 1);
}

void command_nlst(char *dirname)
{
	do_dirlist(dirname, 0);
}

void command_syst(char *params)
{
	control_printf(SL_SUCCESS, "215 UNIX Type: L8");
}

void command_mdtm(char *filename)
{
	struct stat statbuf;
	struct tm *filetime;
	char *fullfilename = bftpd_cwd_mappath(filename);
	if (!stat(fullfilename, (struct stat *) &statbuf)) {
		filetime = gmtime((time_t *) & statbuf.st_mtime);
		control_printf(SL_SUCCESS, "213 %04i%02i%02i%02i%02i%02i",
				filetime->tm_year + 1900, filetime->tm_mon + 1,
				filetime->tm_mday, filetime->tm_hour, filetime->tm_min,
				filetime->tm_sec);
	} else {
		control_printf(SL_FAILURE, "550 Error while determining the modification time: %s",
				strerror(errno));
	}
	free(fullfilename);
}

void command_cwd(char *dir)
{
    if (bftpd_cwd_chdir(dir)) {
		bftpd_log("Error: '%s' while changing directory to '%s'.\n",
				  strerror(errno), dir);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		bftpd_log("Changed directory to '%s'.\n", dir);
		control_printf(SL_SUCCESS, "250 OK");
	}
}

void command_cdup(char *params)
{
	bftpd_log("Changed directory to '..'.\n");
	bftpd_cwd_chdir("..");
	control_printf(SL_SUCCESS, "250 OK");
}

void command_dele(char *filename)
{
	char *mapped = bftpd_cwd_mappath(filename);
        if (pre_write_script)
           run_script(pre_write_script, mapped);

	if (unlink(mapped)) {
		bftpd_log("Error: '%s' while trying to delete file '%s'.\n",
				  strerror(errno), filename);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		bftpd_log("Deleted file '%s'.\n", filename);
		control_printf(SL_SUCCESS, "200 OK");
	}

        if (post_write_script)
           run_script(post_write_script, mapped);

	free(mapped);
}

void command_mkd(char *dirname)
{
	char *mapped = bftpd_cwd_mappath(dirname);

        if (pre_write_script)
           run_script(pre_write_script, mapped);

	if (mkdir(mapped, 0755)) {
		bftpd_log("Error: '%s' while trying to create directory '%s'.\n",
				  strerror(errno), dirname);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		bftpd_log("Created directory '%s'.\n", dirname);
		control_printf(SL_SUCCESS, "257 \"%s\" has been created.", dirname);
	}

        if (post_write_script)
           run_script(post_write_script, mapped);

	free(mapped);
}

void command_rmd(char *dirname)
{
	char *mapped = bftpd_cwd_mappath(dirname);
        if (pre_write_script)
           run_script(pre_write_script, mapped);

	if (rmdir(mapped)) {
		bftpd_log("Error: '%s' while trying to remove directory '%s'.\n", strerror(errno), dirname);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		bftpd_log("Removed directory '%s'.\n", dirname);
		control_printf(SL_SUCCESS, "250 OK");
	}
        
        if (post_write_script)
           run_script(post_write_script, mapped);
	free(mapped);
}

void command_noop(char *params)
{
	control_printf(SL_SUCCESS, "200 OK");
}

void command_rnfr(char *oldname)
{
	FILE *file;
	char *mapped = bftpd_cwd_mappath(oldname);
	if ((file = fopen(mapped, "r"))) {
		fclose(file);
		if (philename)
			free(philename);
		philename = mapped;
		state = STATE_RENAME;
		control_printf(SL_SUCCESS, "350 File exists, ready for destination name");
	} else {
		free(mapped);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	}
}

void command_rnto(char *newname)
{
	char *mapped = bftpd_cwd_mappath(newname);

        if (pre_write_script)
           run_script(pre_write_script, mapped);

	if (rename(philename, mapped)) {
		bftpd_log("Error: '%s' while trying to rename '%s' to '%s'.\n",
				  strerror(errno), philename, bftpd_cwd_mappath(newname));
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		bftpd_log("Successfully renamed '%s' to '%s'.\n", philename, bftpd_cwd_mappath(newname));
		control_printf(SL_SUCCESS, "250 OK");
		state = STATE_AUTHENTICATED;
	}

        if (post_write_script)
           run_script(post_write_script, mapped);

	free(philename);
	free(mapped);
	philename = NULL;
}

void command_rest(char *params)
{
    offset = strtoul(params, NULL, 10);
    control_printf(SL_SUCCESS, "350 Restarting at offset %i.", offset);
}

void command_size(char *filename)
{
	struct stat statbuf;
	char *mapped = bftpd_cwd_mappath(filename);
	if (!stat(mapped, &statbuf)) {
		control_printf(SL_SUCCESS, "213 %i", (int) statbuf.st_size);
	} else {
		control_printf(SL_FAILURE, "550 Error: %s.", strerror(errno));
	}
	free(mapped);
}

void command_quit(char *params)
{
	control_printf(SL_SUCCESS, "221 %s", config_getoption("QUIT_MSG"));
        /* Make sure we log user out. -- Jesse <slicer69@hotmail.com> */
        bftpdutmp_end();
	exit(0);
}

void command_stat(char *filename)
{
	char *mapped = bftpd_cwd_mappath(filename);
    control_printf(SL_SUCCESS, "213-Status of %s:", filename);
    bftpd_stat(mapped, stderr);
    control_printf(SL_SUCCESS, "213 End of Status.");
	free(mapped);
}

/* SITE commands */

void command_chmod(char *params)
{
	int permissions;
	char *mapped;
        char *my_string;

	if (!strchr(params, ' ')) {
		control_printf(SL_FAILURE, "550 Usage: SITE CHMOD <permissions> <filename>");
		return;
	}
        my_string = strdup(strchr(params, ' ') + 1);
        if (! my_string)
        {
            control_printf(SL_FAILURE, "550: An error occured on the server trying to CHMOD.");
            return;
        }
	/* mapped = bftpd_cwd_mappath(strdup(strchr(params, ' ') + 1)); */
        mapped = bftpd_cwd_mappath(my_string);
        free(my_string);
       
        if (pre_write_script)
           run_script(pre_write_script, mapped);

	*strchr(params, ' ') = '\0';
	sscanf(params, "%o", &permissions);
	if (chmod(mapped, permissions))
		control_printf(SL_FAILURE, "Error: %s.", strerror(errno));
	else {
		bftpd_log("Changed permissions of '%s' to '%o'.\n", mapped,
				  permissions);
		control_printf(SL_SUCCESS, "200 CHMOD successful.");
	}
        if (post_write_script)
           run_script(post_write_script, mapped);

	free(mapped);
}

void command_chown(char *params)
{
	char foo[MAXCMD + 1], owner[MAXCMD + 1], group[MAXCMD + 1],
		filename[MAXCMD + 1], *mapped;
	int uid, gid;
	if (!strchr(params, ' ')) {
		control_printf(SL_FAILURE, "550 Usage: SITE CHOWN <owner>[.<group>] <filename>");
		return;
	}
	sscanf(params, "%[^ ] %s", foo, filename);
	if (strchr(foo, '.'))
		sscanf(foo, "%[^.].%s", owner, group);
	else {
		strcpy(owner, foo);
		group[0] = '\0';
	}
	if (!sscanf(owner, "%i", &uid))	/* Is it a number? */
		if (((uid = mygetpwnam(owner, passwdfile))) < 0) {
			control_printf(SL_FAILURE, "550 User '%s' not found.", owner);
			return;
		}
	if (!sscanf(group, "%i", &gid))
		if (((gid = mygetpwnam(group, groupfile))) < 0) {
			control_printf(SL_FAILURE, "550 Group '%s' not found.", group);
			return;
		}
	mapped = bftpd_cwd_mappath(filename);
        if (pre_write_script)
           run_script(pre_write_script, mapped);

	if (chown(mapped, uid, gid))
		control_printf(SL_FAILURE, "550 Error: %s.", strerror(errno));
	else {
		bftpd_log("Changed owner of '%s' to UID %i GID %i.\n", filename, uid,
				  gid);
		control_printf(SL_SUCCESS, "200 CHOWN successful.");
	}
        if (post_write_script)
           run_script(post_write_script, mapped);
	free(mapped);
}

void command_site(char *str)
{
	const struct command subcmds[] = {
		{"chmod ", NULL, command_chmod, STATE_AUTHENTICATED},
		{"chown ", NULL, command_chown, STATE_AUTHENTICATED},
		{NULL, NULL, 0},
	};
	int i;
	if (!strcasecmp(config_getoption("ENABLE_SITE"), "no")) {
		control_printf(SL_FAILURE, "550 SITE commands are disabled.");
		return;
	}
	for (i = 0; subcmds[i].name; i++) {
		if (!strncasecmp(str, subcmds[i].name, strlen(subcmds[i].name))) {
			cutto(str, strlen(subcmds[i].name));
			subcmds[i].function(str);
			return;
		}
	}
	control_printf(SL_FAILURE, "550 Unknown command: 'SITE %s'.", str);
}

void command_auth(char *type)
{
    control_printf(SL_FAILURE, "550 Not implemented yet\r\n");
}

/* Command parsing */

const struct command commands[] = {
	{"USER", "<sp> username", command_user, STATE_CONNECTED, 0},
	{"PASS", "<sp> password", command_pass, STATE_USER, 0},
    {"XPWD", "(returns cwd)", command_pwd, STATE_AUTHENTICATED, 1},
	{"PWD", "(returns cwd)", command_pwd, STATE_AUTHENTICATED, 0},
	{"TYPE", "<sp> type-code (A or I)", command_type, STATE_AUTHENTICATED, 0},
	{"PORT", "<sp> h1,h2,h3,h4,p1,p2", command_port, STATE_AUTHENTICATED, 0},
    {"EPRT", "<sp><d><net-prt><d><ip><d><tcp-prt><d>", command_eprt, STATE_AUTHENTICATED, 1},
	{"PASV", "(returns address/port)", command_pasv, STATE_AUTHENTICATED, 0},
    {"EPSV", "(returns address/post)", command_epsv, STATE_AUTHENTICATED, 1},
    {"ALLO", "<sp> size", command_allo, STATE_AUTHENTICATED, 1},
	{"STOR", "<sp> pathname", command_stor, STATE_AUTHENTICATED, 0},
    {"APPE", "<sp> pathname", command_appe, STATE_AUTHENTICATED, 1},
	{"RETR", "<sp> pathname", command_retr, STATE_AUTHENTICATED, 0},
	{"LIST", "[<sp> pathname]", command_list, STATE_AUTHENTICATED, 0},
	{"NLST", "[<sp> pathname]", command_nlst, STATE_AUTHENTICATED, 0},
	{"SYST", "(returns system type)", command_syst, STATE_CONNECTED, 0},
	{"MDTM", "<sp> pathname", command_mdtm, STATE_AUTHENTICATED, 1},
    {"XCWD", "<sp> pathname", command_cwd, STATE_AUTHENTICATED, 1},
	{"CWD", "<sp> pathname", command_cwd, STATE_AUTHENTICATED, 0},
    {"XCUP", "(up one directory)", command_cdup, STATE_AUTHENTICATED, 1},
	{"CDUP", "(up one directory)", command_cdup, STATE_AUTHENTICATED, 0},
	{"DELE", "<sp> pathname", command_dele, STATE_AUTHENTICATED, 0},
    {"XMKD", "<sp> pathname", command_mkd, STATE_AUTHENTICATED, 1},
	{"MKD", "<sp> pathname", command_mkd, STATE_AUTHENTICATED, 0},
    {"XRMD", "<sp> pathname", command_rmd, STATE_AUTHENTICATED, 1},
	{"RMD", "<sp> pathname", command_rmd, STATE_AUTHENTICATED, 0},
	{"NOOP", "(no operation)", command_noop, STATE_AUTHENTICATED, 0},
	{"RNFR", "<sp> pathname", command_rnfr, STATE_AUTHENTICATED, 0},
	{"RNTO", "<sp> pathname", command_rnto, STATE_RENAME, 0},
	{"REST", "<sp> byte-count", command_rest, STATE_AUTHENTICATED, 1},
	{"SIZE", "<sp> pathname", command_size, STATE_AUTHENTICATED, 1},
	{"QUIT", "(close control connection)", command_quit, STATE_CONNECTED, 0},
	{"HELP", "[<sp> command]", command_help, STATE_AUTHENTICATED, 0},
    {"STAT", "<sp> pathname", command_stat, STATE_AUTHENTICATED, 0},
	{"SITE", "<sp> string", command_site, STATE_AUTHENTICATED, 0},
    {"FEAT", "(returns list of extensions)", command_feat, STATE_AUTHENTICATED, 0},
/*    {"AUTH", "<sp> authtype", command_auth, STATE_CONNECTED, 0},
*/    {"ADMIN_LOGIN", "(admin)", command_adminlogin, STATE_CONNECTED, 0},
      {"MGET", "<sp> pathname", command_mget, STATE_AUTHENTICATED, 0},
      {"MPUT", "<sp> pathname", command_mput, STATE_AUTHENTICATED, 0},
	{NULL, NULL, NULL, 0, 0}
};

void command_feat(char *params)
{
    int i;
    control_printf(SL_SUCCESS, "211-Extensions supported:");
    for (i = 0; commands[i].name; i++)
        if (commands[i].showinfeat)
            control_printf(SL_SUCCESS, " %s", commands[i].name);
    control_printf(SL_SUCCESS, "211 End");
}

void command_help(char *params)
{
	int i;
	if (params[0] == '\0') {
		control_printf(SL_SUCCESS, "214-The following commands are recognized.");
		for (i = 0; commands[i].name; i++)
			control_printf(SL_SUCCESS, "214-%s", commands[i].name);
        control_printf(SL_SUCCESS, "214 End of help");
	} else {
		for (i = 0; commands[i].name; i++)
			if (!strcasecmp(params, commands[i].name))
				control_printf(SL_SUCCESS, "214 Syntax: %s", commands[i].syntax);
	}
}

int parsecmd(char *str)
{
	int i;
	char *p, *pp, confstr[64]; /* strlen("ALLOWCOMMAND_XXXX") + 1 == 18 */
	p = pp = str;			/* Remove garbage in the string */
	while (*p)
		if ((unsigned char) *p < 32)
			p++;
		else
			*pp++ = *p++;
	*pp++ = 0;
	for (i = 0; commands[i].name; i++) {	/* Parse command */
		if (!strncasecmp(str, commands[i].name, strlen(commands[i].name))) {
            sprintf(confstr, "ALLOWCOMMAND_%s", commands[i].name);
            if (!strcasecmp(config_getoption(confstr), "no")) {
                control_printf(SL_FAILURE, "550 The command '%s' is disabled.",
                        commands[i].name);
                return 1;
            }
			cutto(str, strlen(commands[i].name));
			p = str;
			while ((*p) && ((*p == ' ') || (*p == '\t')))
				p++;
			memmove(str, p, strlen(str) - (p - str) + 1);
			if (state >= commands[i].state_needed) {
				commands[i].function(str);
				return 0;
			} else {
				switch (state) {
					case STATE_CONNECTED: {
						control_printf(SL_FAILURE, "503 USER expected.");
						return 1;
					}
					case STATE_USER: {
						control_printf(SL_FAILURE, "503 PASS expected.");
						return 1;
					}
					case STATE_AUTHENTICATED: {
						control_printf(SL_FAILURE, "503 RNFR before RNTO expected.");
						return 1;
					}
				}
			}
		}
	}
	control_printf(SL_FAILURE, "500 Unknown command: \"%s\"", str);
	return 0;
}


int get_buffer_size(int num_connections)
{
   int buffer_size;

   if (num_connections < 1)
      num_connections = 1;

   buffer_size = xfer_bufsize / num_connections;
   if ( buffer_size < 2)
      buffer_size = 2;

   return buffer_size;
}



/*
This function forks and runs a script. On success it
returns TRUE, if an error occures, it returns FALSE.
*/
int run_script(char *script, char *path)
{
   pid_t process_id;
   char *command_args[] = { script, path, NULL } ;
   /* sighandler_t save_quit, save_int, save_chld; */
   sig_t save_quit, save_int, save_chld;

   /* save original signal handler values */
   save_quit = signal(SIGQUIT, SIG_IGN);
   save_int = signal(SIGINT, SIG_IGN);
   save_chld = signal(SIGCHLD, SIG_DFL);

   process_id = fork();
   /* check for failure */
   if (process_id < 0)
   {
      signal(SIGQUIT, save_quit);
      signal(SIGINT, save_int);
      signal(SIGCHLD, save_chld);
      return FALSE;
   }

   /* child process */
   if (process_id == 0)
   {
      signal(SIGQUIT, SIG_DFL);
      signal(SIGINT, SIG_DFL);
      signal(SIGCHLD, SIG_DFL);

      execv(script, command_args);
      bftpd_log("Error trying to run script: %s\n", script);
      exit(127);
   }

   /* parent process */
   do
   {
       process_id = wait4(process_id, NULL, 0, NULL);
   } while ( (process_id == -1) && (errno == EINTR) );

   signal(SIGQUIT, save_quit);
   signal(SIGINT, save_int);
   signal(SIGCHLD, save_chld);

   return TRUE;
}

