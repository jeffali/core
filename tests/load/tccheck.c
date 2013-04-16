#include <stdio.h>
#include <stdint.h>

#define SWAB64(TC_num) \
  ( \
   ((TC_num & 0x00000000000000ffULL) << 56) | \
   ((TC_num & 0x000000000000ff00ULL) << 40) | \
   ((TC_num & 0x0000000000ff0000ULL) << 24) | \
   ((TC_num & 0x00000000ff000000ULL) << 8) | \
   ((TC_num & 0x000000ff00000000ULL) >> 8) | \
   ((TC_num & 0x0000ff0000000000ULL) >> 24) | \
   ((TC_num & 0x00ff000000000000ULL) >> 40) | \
   ((TC_num & 0xff00000000000000ULL) >> 56) \
  )

int main() {
  FILE *fd;
  int ret; int i;
  unsigned long size = 0;
  char *MAGIC="ToKyO CaBiNeT";
  fd = fopen("/tmp/a.tcdb", "r");
  //struct stat st;
  //fstat(fd, &st);
  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  printf("size=%ld len=%ld\n", size, strlen(MAGIC));
  char hbuf[256];
  memset(hbuf, 0, (size_t)256);
  fseek(fd, 0, SEEK_SET);
  if(1==fread(&hbuf, 256, 1, fd)) {
   hbuf[14]='\0';
   printf("%s\n", hbuf);
  }
  //
  uint64_t sz = 0;
  memcpy(&sz, hbuf+56, sizeof(uint64_t));
  printf("%llu\n", sz);
  sz = SWAB64(sz);
  printf("%llu\n", sz);
  fclose(fd);
  return 0;
}
