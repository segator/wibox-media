#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
int main(){
  int fd=open("/dev/gk_video",2); if(fd<0){perror("open");return 1;}
  unsigned int v,r;
  printf("1 ");
  r=ioctl(fd,_IOR(0x76,0x52,unsigned int),&v);
  printf("ioctl=%d v=0x%x %s\n",r,v,strerror(errno));
  printf("2 ");
  r=ioctl(fd,_IOW(0x76,0x54,unsigned int),0);
  printf("ioctl=%d %s\n",r,strerror(errno));
  printf("3 ");
  r=ioctl(fd,_IOR(0x76,0x70,unsigned int),&v);
  printf("ioctl=%d v=0x%x %s\n",r,v,strerror(errno));
  printf("DONE\n"); close(fd); return 0;
}
