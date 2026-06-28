#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#define GKIOR(t,n)  _IOR(t,n,unsigned int)
#define GKIOW(t,n)  _IOW(t,n,unsigned int)
#define DO(n,r,a) { int _r=ioctl(gk,r,(unsigned long)(a)); fprintf(stderr," %s: %s\n",n,_r?strerror(errno):"OK"); }

int main(){
  int gk=open("/dev/gk_video",O_RDWR);
  if(gk<0){perror("open");return 1;}

  /* Allocate on heap to avoid stack corruption */
  unsigned int v;
  unsigned char *buf = malloc(65536);
  memset(buf, 0, 65536);

  /* Version */
  DO("R",  GKIOR(0x76,0x52), &v);
  DO("T",  GKIOW(0x76,0x54), buf);
  DO("p",  GKIOR(0x76,0x70), &v);
  DO("t",  GKIOR(0x76,0x74), &v);
  DO("s_c",GKIOW(0x76,0x73), buf);

  /* SYS */
  DO("16", GKIOW(0x73,0x16), buf);
  DO("01", GKIOR(0x73,0x01), &v);
  DO("04", GKIOR(0x73,0x04), &v);
  DO("0b", GKIOW(0x73,0x0b), buf);

  /* Media */
  DO("m5", GKIOR(0x6d,0x05), &v);
  DO("m0", GKIOR(0x6d,0x00), &v);
  DO("m4", GKIOR(0x6d,0x04), &v);
  DO("m10",GKIOR(0x6d,0x10), &v);
  DO("m20",GKIOR(0x6d,0x20), &v);

  /* Sub */
  DO("i20",GKIOR(0x69,0x20), &v);
  DO("i21",GKIOW(0x69,0x21), buf);

  /* Audio */
  DO("pcm",GKIOR(0x50,0x02), &v);
  fprintf(stderr,"  rate=0x%x\n",v);

  fprintf(stderr,"\nALL OK — no crash\n");
  //keep_buf
  close(gk);
  fprintf(stderr,"DONE\n");
  _exit(0);
}
