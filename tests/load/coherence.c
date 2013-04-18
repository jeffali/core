#include <stdio.h>
#include <stdint.h>

int main() {
  FILE *fp;
  fp = fopen("/tmp/a.tcdb", "r");
  char hbuf[256];
  fread(&hbuf, 256, 1, fp);
  uint64_t sz;
  uint64_t fr;
  uint64_t nr;
  uint64_t nb;
  uint8_t  apow;

  memcpy(&apow, hbuf+34, sizeof(uint8_t));
  memcpy(&nb,   hbuf+40, sizeof(uint64_t));
  memcpy(&nr,   hbuf+48, sizeof(uint64_t));
  memcpy(&sz,   hbuf+56, sizeof(uint64_t));
  memcpy(&fr,   hbuf+64, sizeof(uint64_t));
  
  printf("apow=%u, nb=%llu nr=%llu sz=%llu fr=%llu\n", apow, nb, nr, sz, fr);  
  
  // LOOP OVER BUCKET SECTION
  uint64_t off = 256; // + nb * sizeof(uint64_t);
  uint32_t pv;
  while(off < fr) {
    fseek(fp, off, SEEK_SET);
    fread(&pv, sizeof(uint32_t), 1, fp); // 64 for large mode
    if(pv) {
       //pv = pv << apow;
       printf("off=%lld, pv=%llx\n", off, pv);
    }
    off += sizeof(uint32_t); //sizeof(uint64_t) in LARGE mode
  }

  // LOOP OVER RECORDS SECTION
  uint64_t ofr = fr; //+ 14 + 9 +1 +0x18;
  uint16_t pad = 0;
  char rbuf[14];
  memset(&rbuf, 0, 14);
  while(ofr < sz) {
    fseek(fp, ofr, SEEK_SET);
    fread(&rbuf, 14, 1, fp); // + map+ksz+vsz
    printf("flag=%x, hash=%u, pad=%x, ksz=%u, vsz=%u\n", (unsigned char)rbuf[0],(unsigned char)rbuf[1], (unsigned char)rbuf[10], pad, (unsigned char)rbuf[12], (unsigned char)rbuf[13]);
    printf("pad=%x\n", (unsigned char)rbuf[10]);
    printf("ksz=%x\n", (unsigned char)rbuf[12]);
    printf("vsz=%x\n", (unsigned char)rbuf[13]);
    ofr+= 14 + (unsigned char)rbuf[10]+ (unsigned char)rbuf[12] +(unsigned char)rbuf[13];
  }
  fclose(fp);
}
