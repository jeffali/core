#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse(FILE *fp)
{
     char *s;
     char buf[2048];
     int ignorenext = 0;
     while((s = fgets(buf, 2048 , fp))!=NULL)
     {
       if(strcspn(s, "\r\n") == strlen(s))
       {
         ignorenext = 1;
       } else {
         if(ignorenext==1) {
           ignorenext=0;
         } else {
           printf("S[at %ld]=[%s]\n", ftell(fp), buf);
         }
       }
     }

#if 0
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
#endif
}
int main()
{
     FILE *fp;
     fp = fopen("/tmp/d.txt", "r");
     parse(fp);
     fclose(fp);

     return 0;


     char *s;
     char buf[2048];
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     s = fgets(buf, 2048 , fp);
     printf("S=[%s]\n", buf);
     fclose(fp);
}
