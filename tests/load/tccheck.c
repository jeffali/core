#include <stdio.h>
#include <stdint.h>

#define SWAB64(num) \
  ( \
   ((num & 0x00000000000000ffULL) << 56) | \
   ((num & 0x000000000000ff00ULL) << 40) | \
   ((num & 0x0000000000ff0000ULL) << 24) | \
   ((num & 0x00000000ff000000ULL) << 8) | \
   ((num & 0x000000ff00000000ULL) >> 8) | \
   ((num & 0x0000ff0000000000ULL) >> 24) | \
   ((num & 0x00ff000000000000ULL) >> 40) | \
   ((num & 0xff00000000000000ULL) >> 56) \
  )

int main() {
  FILE *fd;
  int ret; int i;
  unsigned long size = 0;
  char *MAGIC="ToKyO CaBiNeT";
  fd = fopen("/tmp/a.tcdb", "r");
  if(!fd) {printf("error opening\n");}
  //struct stat st;
  //fstat(fd, &st);
  ret=fseek(fd, 0, SEEK_END);
  if(ret!=0) {printf("error seeking\n");} 
  size = ftell(fd);
  printf("size=%ld len=%ld\n", size, strlen(MAGIC));
  if(size<256) {printf("BIG EXCEPTIOn (minimal size)\n");}
  char hbuf[256];
  memset(hbuf, 0, (size_t)256);
  ret=fseek(fd, 0, SEEK_SET);
  if(ret!=0) {printf("error seeking\n");} 
  if(1==fread(&hbuf, 256, 1, fd)) {
   hbuf[14]='\0';
   printf("%s\n", hbuf);
  } else {
    printf("error reading\n");
  }
  //
  if(strncmp(hbuf, MAGIC, strlen(MAGIC))!=0) {
    printf("BIG EXCEPTIOn (magic string)\n");
  }
  //
  uint64_t sz = 0;
  memcpy(&sz, hbuf+56, sizeof(uint64_t));
  printf("%llu\n", sz);
  if(sz==size) {
    printf("OK (seems to be valid)\n");
  } else {
    sz = SWAB64(sz);
    printf("%llu\n", sz);
    if(sz==size) {
      printf("BIG EXCEPTIOn (indianness mismatch)\n");
    } else {
      printf("BIG EXCEPTIOn (size mismatch)\n");
    }
  }
  ret=fclose(fd);
  if(ret!=0) {printf("problem closing file: %s\n", strerror("fclose"));}
  return 0;
}
