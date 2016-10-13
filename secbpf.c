/*
 * Copyright (c) 2016 Christian S.J. Peron
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012 Will Drewry <wad@dataspill.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef linux
#ifdef SECCOMP_FILTER_DEBUG
/* Use the kernel headers in case of an older toolchain. */
# include <asm/siginfo.h>
# define __have_siginfo_t 1
# define __have_sigval_t 1
# define __have_sigevent_t 1
#endif /* SECCOMP_FILTER_DEBUG */
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <linux/net.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <elf.h>

#include <asm/unistd.h>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>  /* for offsetof */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "secbpf.h"

/* Linux seccomp_filter sandbox */
#define SECCOMP_FILTER_FAIL SECCOMP_RET_KILL

/* Use a signal handler to emit violations when debugging */
#ifdef SECCOMP_FILTER_DEBUG
# undef SECCOMP_FILTER_FAIL
# define SECCOMP_FILTER_FAIL SECCOMP_RET_TRAP
#endif /* SECCOMP_FILTER_DEBUG */

/* Syscall filtering set for preauth. */
static const struct sock_filter preauth_insns[] = {
	/* Ensure the syscall arch convention is as expected. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, arch)),
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, SECCOMP_AUDIT_ARCH, 1, 0),
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
	/* Load the syscall number for checking. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, nr)),
	/* Syscalls to permit */
#ifdef __NR_accept
	SC_ALLOW(accept),
#endif
#ifdef __NR_brk
	SC_ALLOW(brk),
#endif
#ifdef __NR_close
	SC_ALLOW(close),
#endif
#ifdef __NR_exit
	SC_ALLOW(exit),
#endif
#ifdef __NR_exit_group
	SC_ALLOW(exit_group),
#endif
#ifdef __NR_read
	SC_ALLOW(read),
#endif
#ifdef __NR_rt_sigprocmask
	SC_ALLOW(rt_sigprocmask),
#endif
#ifdef __NR_shutdown
	SC_ALLOW(shutdown),
#endif
#ifdef __NR_sigprocmask
	SC_ALLOW(sigprocmask),
#endif
#ifdef __NR_time
	SC_ALLOW(time),
#endif
#ifdef __NR_write
	SC_ALLOW(write),
#endif
#ifdef __NR_sendmsg
	SC_ALLOW(sendmsg),
#endif
#ifdef __NR_recvfrom
	SC_ALLOW(recvfrom),
#endif
#ifdef __NR_recvmsg
	SC_ALLOW(recvmsg),
#endif
	/* threading related syscalls here */
#ifdef	__NR_mmap
	SC_ALLOW(mmap),
#endif
#ifdef	__NR_mprotect
	SC_ALLOW(mprotect),
#endif
#ifdef	__NR_munmap
	SC_ALLOW(munmap),
#endif
#ifdef __NR_getpeername
	SC_ALLOW(getpeername),
#endif
#ifdef __NR_clone
	SC_ALLOW(clone),
#endif
#ifdef __NR_futex
	SC_ALLOW(futex),
#endif
#ifdef __NR_faccessat
	SC_ALLOW(faccessat),
#endif
#ifdef __NR_set_robust_list
	SC_ALLOW(set_robust_list),
#endif
#ifdef __NR_get_robust_list
	SC_ALLOW(get_robust_list),
#endif
#ifdef __NR_getsockname
	SC_ALLOW(getsockname),
#endif
#ifdef __NR_fcntl
	/*
	 * NB: fcntl() and fstat is allowed in capsicum (FreeBSDS) however we probably
	 * want to do some additional scoping based on arguments.
	 */
	SC_ALLOW(fcntl),
	SC_ALLOW(fstat),
	SC_ALLOW(lseek),
	SC_ALLOW(ioctl),
#endif
#ifdef __NR_writev
	SC_ALLOW(writev),
#endif
	/* Default deny */
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
};

static const struct sock_fprog preauth_program = {
	.len = (unsigned short)(sizeof(preauth_insns)/sizeof(preauth_insns[0])),
	.filter = (struct sock_filter *)preauth_insns,
};

#ifdef SECCOMP_FILTER_DEBUG
static void
sandbox_violation(int signum, siginfo_t *info, void *void_context)
{

	(void) fprintf(stdout,
	    "%s: unexpected system call (arch:0x%x,syscall:%d @ %p)\n",
	    __func__, info->si_arch, info->si_syscall, info->si_call_addr);
	_exit(1);
}

static void
seccomp_debugging(void)
{
	struct sigaction act;
	sigset_t mask;

	memset(&act, 0, sizeof(act));
	sigemptyset(&mask);
	sigaddset(&mask, SIGSYS);

	act.sa_sigaction = &sandbox_violation;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSYS, &act, NULL) == -1) {
		(void) fprintf(stderr, "%s: sigaction(SIGSYS): %s",
		    __func__, strerror(errno));
		_exit(0);
	}
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
		(void) fprintf(stderr, "%s: sigprocmask(SIGSYS): %s",
		      __func__, strerror(errno));
		_exit(0);
	}
}
#endif /* SECCOMP_FILTER_DEBUG */

void
seccomp_activate(void)
{
#ifdef SECCOMP_FILTER_DEBUG
	seccomp_debugging();
#endif /* SECCOMP_FILTER_DEBUG */
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
		(void) fprintf(stderr, "prctl(PR_SET_NO_NEW_PRIVS): %s\n",
		    strerror(errno));
		exit(1);
	}
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &preauth_program) == -1) {
		(void) fprintf(stderr, "PR_SET_SECCOMP failed: %s\n",
		    strerror(errno));
		exit(1);
	}
}
#endif /* linux */
