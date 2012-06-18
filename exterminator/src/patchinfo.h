#ifndef __PATCHINFO_H__
#define __PATCHINFO_H__

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// XXX: make HASH a power of 2, use bit ops to hash, ensure that callsites aren't biased

#define HASH_SIZE 1021

void get_string_bit(char * bit, char * buf, int & idx) {
  int i = 0;
  for(; !isspace(buf[idx]); idx++) {
    bit[i++] = buf[idx];
  }
  bit[i++] = '\0';
  while(isspace(buf[idx])) {
    idx++;
  }
}

class PatchInfo {
 public:
  PatchInfo() {}

  void init() {
    readFile();
  }

  size_t getPadding(unsigned long callsite) {
    return overflow_hash[hash(callsite)];
  }

  size_t getLifeExtension(unsigned long alloc_site, unsigned long free_site) {
    return dangle_hash[hash(alloc_site ^ free_site)];
  }
 private:
  inline unsigned int hash(unsigned long callsite) {
    return callsite % HASH_SIZE;
  }

  void readFile() {
    const char * fname = getenv("DH_PATCHFILE");
    //fprintf(stderr,"reading file %s\n",fname);
    if(!fname) {
      fprintf(stderr,"can't open file\n");
      return;
    }
    
    //fprintf(stderr,"about to open file\n");
    int fd = open(fname,O_RDONLY);
    //fprintf(stderr,"file opened fd = %d\n",fd);
    if(fd == -1) {
      return;
    }

    char buf[1024];
    
    int ct;
    int which_bit = 0;
    int idx = 0;

    ct = read(fd,buf,1024);
    /*
    do {
      // TODO FIXME XXX: fix this possible buffer overflow
      for(int i = 0; i < ct; i++) {
        // XXX: won't work on windows (crlf) problem with 2 byte newline
        if(isspace(buf[i])) {
          fprintf(stderr,"next word\n");
          bits[which_bit][idx++] = 0;
          which_bit = (which_bit+1)%4;
          idx = 0;
          if(which_bit == 0) {
            if(!strcmp("overflow",bits[0])) {
              unsigned long site = (unsigned)strtoll(bits[1],NULL,16);
              int pad =  strtol(bits[2],NULL,10);
              overflow_hash[hash(site)] = pad;
              fprintf(stderr,"overflow of %d at 0x%x\n",pad,site);
            } 
            else if(!strcmp("dangle",bits[0])) {
              unsigned long alloc_site = (unsigned)strtoll(bits[1],NULL,16);
              int td = strtol(bits[2],NULL,10);
              dangle_hash[hash(alloc_site)] = td;
              fprintf(stderr,"dangle of %d at 0x%x\n",td,alloc_site);
            }
            else {
              fprintf(stderr,"invalid patch type code: %s\n",bits[0]);
            }
          }
        }
        else {
          fputc(buf[i],stderr);
          bits[which_bit][idx++] = buf[i];
        }
      }
    } while(ct = read(fd,buf,1024));
    */

    for(int idx = 0; idx < ct;) {
      char bit[1024];
      get_string_bit(bit,buf,idx);
      if(!strcmp("overflow",bit)) {
        get_string_bit(bit,buf,idx);
        unsigned long site = (unsigned)strtoll(bit,NULL,16);
        get_string_bit(bit,buf,idx);
        int pad =  strtol(bit,NULL,10);
        overflow_hash[hash(site)] = pad;
        fprintf(stderr,"overflow of %d at 0x%x\n",pad,site);
      } else if(!strcmp("dangle",bit)) {
        get_string_bit(bit,buf,idx);
        unsigned long alloc_site = (unsigned)strtoll(bit,NULL,16);
        get_string_bit(bit,buf,idx);
        unsigned long free_site  = (unsigned)strtoll(bit,NULL,16);
        get_string_bit(bit,buf,idx);
        int td = strtol(bit,NULL,10);
        dangle_hash[hash(alloc_site ^ free_site)] = td;
        fprintf(stderr,"dangle of %d at 0x%x, 0x%x\n",td,alloc_site,free_site);
      } else if(!strcmp("stat",bit)) {
        // skip alloc site, free site, heads, tails
        get_string_bit(bit,buf,idx);
        get_string_bit(bit,buf,idx);
        get_string_bit(bit,buf,idx);
        get_string_bit(bit,buf,idx);
      }
    }

    close(fd);
  }

  size_t overflow_hash[HASH_SIZE];
  int    dangle_hash  [HASH_SIZE];
};

#endif // __PATCHINFO_H__
