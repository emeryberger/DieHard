#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/dir.h>
#include "mail.h"
#include "osdep.h"
#include "carmel2.h"
#include "carmel.h"

char *last_path(), *cpystr();
void carmel2_bezerk_mail();
extern DRIVER carmeldriver;

extern char carmel_20k_buf[20000];


main(argc, argv)
     int argc;
     char **argv;
{
    char carmel_folder[500], *bezerk_folder;
    MAILSTREAM *stream;

    if(argc <= 1) {
        fprintf(stderr, "Usage: bzk2cml <folder>\n");
        exit(-1);
    }

    for(argv++; *argv != NULL; argv++) {
        bezerk_folder = last_path(*argv);  /* Only last component of path */
        sprintf(carmel_folder, "#carmel#%s", bezerk_folder);

        if(carmel_create(NULL, carmel_folder) == 0) {
            continue;
        }


        stream          = (MAILSTREAM *)fs_get(sizeof(MAILSTREAM));
        stream->dtb     = &carmeldriver;
        stream->mailbox = cpystr(carmel_folder);
	stream->local   = NULL;

        if(carmel_open(stream) == NULL) {
            continue;
        }

        if(carmel2_lock(stream, stream->mailbox, WRITE_LOCK) < 0) {
            fprintf(stderr, "Carmel folder %s locked\n", carmel_folder);
            carmel_close(stream);
            continue;
        }
          
        carmel2_spool_mail(stream, *argv, stream->mailbox, 0);
        carmel2_unlock(stream, stream->mailbox, WRITE_LOCK);
        fprintf(stderr, "Folder \"%s\" copied to \"%s\"\n", *argv,
                carmel_folder);
        carmel_close(stream);
    }
}
            

        
        
        

    

char *last_path(path)
     char *path;
{
    char *p;

    p = path + strlen(path) - 1;

    while(p > path && *p != '/')
      p--;

    if(*p == '/')
      p++;
    return(p);
}
     
void
mm_log(mess, code)
     char *mess;
     int code;
{
    fprintf(stderr, "%s\n", mess);
}

void
mm_fatal()
{
    abort();
}

void mm_mailbox() {}
void mm_critical() {}
void mm_nocritical() {}
void mm_searched() {}
void mm_exists() {}
void mm_expunged() {}

