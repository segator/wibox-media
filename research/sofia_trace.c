#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <asm/ptrace.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define LOG_PATH "/mnt/mtd/iotrace_boot.log"
#define MAX_SEGV 50
#define MAX_TRACED 64

static FILE *logf;
static int segv_count;
static long nsyscalls;

/* per-thread state */
static pid_t traced_pids[MAX_TRACED];
static int    traced_in_syscall[MAX_TRACED];
static int    traced_prev_sysnr[MAX_TRACED];
static int    ntraced;

static int find_slot(pid_t pid) {
	int i;
	for (i = 0; i < ntraced; i++) if (traced_pids[i] == pid) return i;
	return -1;
}
static int add_slot(pid_t pid) {
	if (ntraced >= MAX_TRACED) return -1;
	traced_pids[ntraced] = pid;
	traced_in_syscall[ntraced] = 0;
	traced_prev_sysnr[ntraced] = -1;
	return ntraced++;
}

static void log_msg(const char *fmt, ...) {
	if (!logf) return;
	va_list ap; va_start(ap, fmt); vfprintf(logf, fmt, ap); va_end(ap);
	fflush(logf);
}

int main(int argc, char *argv[]) {
	if (argc < 2) { fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]); return 1; }
	logf = fopen(LOG_PATH, "w");
	if (!logf) { perror(LOG_PATH); logf = stderr; }
	log_msg("=== sofia_trace v2: multi-thread ioctl tracer ===\n");

	pid_t main_child = fork();
	if (main_child == 0) {
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execv(argv[1], argv + 1);
		_exit(1);
	}

	int status;
	pid_t child;
	segv_count = 0; nsyscalls = 0; ntraced = 0;

	/* Wait for initial exec stop */
	waitpid(main_child, &status, 0);
	if (!WIFSTOPPED(status)) {
		log_msg("[tracer] child died before first stop\n");
		return 1;
	}
	ptrace(PTRACE_SETOPTIONS, main_child, NULL,
		PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK);
	add_slot(main_child);
	log_msg("[tracer] main PID=%d, tracing threads...\n", main_child);
	struct pt_regs regs;
	ptrace(PTRACE_GETREGS, main_child, NULL, &regs);
	log_msg("[tracer] initial PC=0x%08lx\n", regs.ARM_pc);
	ptrace(PTRACE_SYSCALL, main_child, NULL, 0);

	while (1) {
		child = waitpid(-1, &status, __WALL);
		if (child == -1) {
			if (errno == ECHILD) break;
			continue;
		}

		if (WIFEXITED(status)) {
			log_msg("[tracer] PID %d exited %d\n", child, WEXITSTATUS(status));
			if (child == main_child) break;
			continue;
		}
		if (WIFSIGNALED(status)) {
			log_msg("[tracer] PID %d signal %d\n", child, WTERMSIG(status));
			if (child == main_child) break;
			continue;
		}
		if (!WIFSTOPPED(status)) continue;

		int sig = WSTOPSIG(status);
		int event = status >> 16;
		int slot = find_slot(child);

		/* New thread/process */
		if (event == PTRACE_EVENT_CLONE || event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_VFORK) {
			unsigned long newpid;
			ptrace(PTRACE_GETEVENTMSG, child, NULL, &newpid);
			if (add_slot((pid_t)newpid) >= 0) {
				log_msg("[tracer] new child PID=%lu\n", newpid);
			}
			ptrace(PTRACE_SYSCALL, child, NULL, 0);
			continue;
		}
		if (slot < 0) {
			/* Untraced child (shouldn't happen) */
			ptrace(PTRACE_SYSCALL, child, NULL, sig == SIGTRAP ? 0 : sig);
			continue;
		}

		if (sig == SIGTRAP) {
			ptrace(PTRACE_GETREGS, child, NULL, &regs);
			if (traced_in_syscall[slot]) {
				/* syscall exit */
				if (traced_prev_sysnr[slot] == 54)
					log_msg("[%d] = 0x%lx\n", child, regs.ARM_r0);
				traced_in_syscall[slot] = 0;
			} else {
				/* syscall entry */
				int nr = (int)regs.ARM_r7;
				traced_prev_sysnr[slot] = nr;
				traced_in_syscall[slot] = 1;
				if (nr == 54) {
					nsyscalls++;
					log_msg("[%d] IOC #%ld fd=%d cmd=0x%08lx arg=0x%08lx",
						child, nsyscalls, (int)regs.ARM_r0, regs.ARM_r1, regs.ARM_r2);
				}
			}
			ptrace(PTRACE_SYSCALL, child, NULL, 0);
		} else if (sig == SIGSEGV || sig == SIGILL || sig == SIGBUS) {
			ptrace(PTRACE_GETREGS, child, NULL, &regs);
			segv_count++;
			log_msg("[%d] SEGV #%d PC=0x%08lx LR=0x%08lx\n",
				child, segv_count, regs.ARM_pc, regs.ARM_lr);
			if (segv_count > MAX_SEGV) {
				log_msg("[tracer] too many SEGVs, killing\n");
				kill(main_child, SIGKILL);
			} else {
				regs.ARM_pc += 4;
				ptrace(PTRACE_SETREGS, child, NULL, &regs);
			}
			ptrace(PTRACE_SYSCALL, child, NULL, 0);
		} else {
			/* Forward other signals */
			ptrace(PTRACE_SYSCALL, child, NULL, sig);
		}
	}
	log_msg("[tracer] done: %ld ioctls, %d SEGVs\n", nsyscalls, segv_count);
	if (logf && logf != stderr) fclose(logf);
	return 0;
}
