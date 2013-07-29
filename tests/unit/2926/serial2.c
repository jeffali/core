#include <stdio.h>
#include <string.h>


#define CF_BUFSIZE 10
int main2()
{
     int maxsize = 9999;
     char *s;
     char buf[CF_BUFSIZE];
     int ignorenext = 0;
     FILE *fp;
     fp = fopen("/tmp/a.txt", "r");
     int hcount = 0;
     int nbbytesread = 0;
     while((s = fgets(buf, CF_BUFSIZE , fp))!=NULL)
     {
       nbbytesread += strlen(s);
       if(strcspn(s, "\r\n") == strlen(s))
       {
         ignorenext = 1;
       } else {
         if(ignorenext==1) {
           ignorenext=0;
         } else {
           if(nbbytesread>maxsize) {
             int delta = nbbytesread - maxsize;
             s[strlen(s)-delta]='\0';
             printf("delta=%d %d %s\n",delta, strlen(s));
           }

           printf("S[at %ld]=[%s]\n", ftell(fp), buf);

#if 0
           if(strcspn(s, "\r\n")>0)
           {
               s[strcspn(s, "\r\n")]='\0';
           }
           //if (strcmp(comment,"")==0 || FullTextMatch(comment, s) == 0 )
           if (strcmp(comment,"")==0 || (s = StripPatterns(s, comment, filename))!=NULL)
           //if (/*LineNotExcluded(s)*/*s!='#')
           {
              if(hcount<maxent-1) {
                int res = ProcessFieldSeparatedLine(ctx, bundle, array_lval, s, 
                                 split, maxent, type, intIndex, hcount);
              } else {
                break;
              }
              ++hcount;
           }
#endif
         }
       }
       if(nbbytesread>maxsize) break;
     }
     fclose(fp);
     return 0;
}
int main0()
{

     char input[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n10cccc\nddddddddddd\nffffffffff\ngggggggggg\nggggg\nyyyy\nlllllllllllllll\nZZZZZZZ\n";
     int maxsize = 9999;
     char *s;
     char buf[CF_BUFSIZE];
     int ignorenext = 0;
     //FILE *fp;
     //fp = fopen("/tmp/a.txt", "r");
     int hcount = 0;
     int nbbytesread = 0;
     while(memcpy(buf, input+nbbytesread, CF_BUFSIZE )!=NULL)
     {
       s=buf;
       nbbytesread += strlen(s);
       if(strcspn(s, "\r\n") == strlen(s))
       {
         ignorenext = 1;
       } else {
         if(ignorenext==1) {
           ignorenext=0;
         } else {
           if(nbbytesread>maxsize) {
             int delta = nbbytesread - maxsize;
             s[strlen(s)-delta]='\0';
             printf("delta=%d %d %s\n",delta, strlen(s));
           }

           printf("S[at %ld]=[%s]\n", -1/*ftell(fp)*/, buf);

#if 0
           if(strcspn(s, "\r\n")>0)
           {
               s[strcspn(s, "\r\n")]='\0';
           }
           //if (strcmp(comment,"")==0 || FullTextMatch(comment, s) == 0 )
           if (strcmp(comment,"")==0 || (s = StripPatterns(s, comment, filename))!=NULL)
           //if (/*LineNotExcluded(s)*/*s!='#')
           {
              if(hcount<maxent-1) {
                int res = ProcessFieldSeparatedLine(ctx, bundle, array_lval, s, 
                                 split, maxent, type, intIndex, hcount);
              } else {
                break;
              }
              ++hcount;
           }
#endif
         }
       }
       if(nbbytesread>maxsize) break;
     }
     //fclose(fp);
     return 0;
}



int main()
{
   //char b[]="aaaa\nbbbb\ncccc\nddddee";

   char b[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n10cccc\nddddddddddd\nffffffffff\ngggggggggg\nggggg\nyyyy\nlllllllllllllll\nZZZZZZZ\n";
   char buf[CF_BUFSIZE+1];
   int n = 0;
   size_t l = strlen(b);
   //printf("L=%d\n", l);

   int j = 4;
   while(1) {
      size_t len = strcspn(b + n, "\n\r");
      strncpy(buf, b+n, len); buf[len]='\0';
      if(len<CF_BUFSIZE) printf("S at[%d] [%s]\n",n+len,buf);
      n += len + 1;
      //printf("N=%d[%d]\n",n,len);
      if(n>=l) break;
   }
   return 0;
}
