#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#define GKIOR(t,n)  _IOR(t,n,unsigned int)
#define GKIOW(t,n)  _IOW(t,n,unsigned int)
#define GKIORW(t,n) _IOWR(t,n,unsigned int)
#define GIOC(t,n)   _IO(t,n)

static int ok(const char *n, int r) {
    if (r) fprintf(stderr,"[!] %s: %s\n",n,strerror(errno));
    else   fprintf(stderr,"  %s OK\n",n);
    return r;
}

int main() {
    unsigned int v;
    unsigned char *buf = malloc(1048576); /* 1MB DMA buffer */
    memset(buf, 0, 1048576);

    /* ── Phase 1: Open GK master ── */
    int gk = open("/dev/gk_video", O_RDWR);
    fprintf(stderr,"GK fd=%d\n", gk);

    /* ── Phase 2: Version queries ── */
    ok("R_ver", ioctl(gk, GKIOR(0x76,0x52), &v));
    ok("T_cfg", ioctl(gk, GKIOW(0x76,0x54), buf));
    ok("p_ver", ioctl(gk, GKIOR(0x76,0x70), &v));
    ok("t_ver", ioctl(gk, GKIOR(0x76,0x74), &v));
    ok("s_cfg", ioctl(gk, GKIOW(0x76,0x73), buf));

    /* ── Phase 3: SYS config ── */
    ok("s16", ioctl(gk, GKIOW(0x73,0x16), buf));
    ok("s01", ioctl(gk, GKIOR(0x73,0x01), &v));
    ok("s04", ioctl(gk, GKIOR(0x73,0x04), &v));
    ok("s0b", ioctl(gk, GKIOW(0x73,0x0b), buf));

    /* ── Phase 4: Media config ── */
    ok("m05", ioctl(gk, GKIOR(0x6d,0x05), &v));
    ok("m00", ioctl(gk, GKIOR(0x6d,0x00), &v));
    ok("m04", ioctl(gk, GKIOR(0x6d,0x04), &v));
    ok("m10", ioctl(gk, GKIOR(0x6d,0x10), &v));
    ok("m20", ioctl(gk, GKIOR(0x6d,0x20), &v));

    /* ── Phase 5: Sub system ── */
    ok("i20", ioctl(gk, GKIOR(0x69,0x20), &v));
    ok("i21", ioctl(gk, GKIOW(0x69,0x21), buf));

    /* ── Phase 6: Audio ── */
    ok("pcm", ioctl(gk, GKIOR(0x50,0x02), &v));

    /* ── Phase 7: VENC channel init (3 channels) ── */
    fprintf(stderr,"\n--- VENC channels ---\n");
    unsigned char *venc_bufs[3];
    int i;
    for (i = 0; i < 3; i++) {
        venc_bufs[i] = buf + 65536 + i * 131072; /* separate buffers per channel */
        ok("V28", ioctl(gk, GKIOW(0x65,0x28), venc_bufs[i]));
        ok("V33", ioctl(gk, GKIOW(0x65,0x33), buf));
        ok("V40", ioctl(gk, GKIORW(0x65,0x40), venc_bufs[i]));
        ok("V3f", ioctl(gk, GKIOW(0x65,0x3f), venc_bufs[i]));
        ok("V38", ioctl(gk, GKIOW(0x65,0x38), buf));
        ok("V45", ioctl(gk, GKIORW(0x65,0x45), buf));
        ok("V44", ioctl(gk, GKIOW(0x65,0x44), buf));
    }

    /* ── Phase 8: Pipeline routing ── */
    fprintf(stderr,"\n--- Pipeline ---\n");
    for (i = 0; i < 4; i++) ok("6f", ioctl(gk, GKIOW(0x6f,0x00), buf));

    /* ── Phase 9: Now try VI ── */
    fprintf(stderr,"\n--- VI init ---\n");
    int vi = open("/dev/gk_video", O_RDWR);
    fprintf(stderr,"VI fd=%d\n", vi);
    ok("VI00", ioctl(vi, GKIOW(0x59,0x00), buf));
    ok("VI30", ioctl(vi, GIOC(0x59,0x30), 0xa0));
    ok("VI32", ioctl(vi, GKIOW(0x59,0x32), buf));

    /* ── Phase 10: VENC start ── */
    fprintf(stderr,"\n--- VENC start ---\n");
    ok("VENC_start 1", ioctl(gk, GKIOW(0x65,0x41), 1));

    fprintf(stderr,"\n=== FULL INIT DONE ===\n");
    close(vi); close(gk);
    _exit(0);
}
