#include <stdio.h>

int main(int  argc, char *argv[]) {
   FILE * dt_in = fopen(argv[1],"rb");
 	unsigned int death_time;

    fread(&death_time,sizeof(unsigned long),1,dt_in);
    fclose(dt_in);
 
   printf("%d",death_time);
return 0;
}
