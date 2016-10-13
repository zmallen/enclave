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
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef __FreeBSD__
#include <sys/capsicum.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>


#include "privsep_libc.h"
#include "edged.h"
#include "privsep.h"
#include "net.h"
#include "privsep_fdpass.h"

#include <stdarg.h>
#include "secbpf.h"
#include "unix.h"
#include "util.h"

volatile pid_t child_pid = -1;
int priv_fd = -1;
int priv_sep_on = 0;
int __real_open(const char *path, int flags, ...);

volatile sig_atomic_t gotsig_chld = 0;

/* Proto-types */
static void sig_pass_to_chld(int);
static void sig_chld(int);
int  may_read(int, void *, size_t);
void must_read(int, void *, size_t);
void must_write(int, void *, size_t);

static void
sig_chld(int sig)
{

	gotsig_chld = 1;
}

/* If priv parent gets a TERM or HUP, pass it through to child instead */
static void
sig_pass_to_chld(int sig)
{
	int oerrno;

	oerrno = errno;
	if (child_pid != -1)
		(void) kill(child_pid, sig);
	errno = oerrno;
}

static void
priv_setuid(void)
{
	struct passwd *pwd;

	/* NB: getuid check is not sufficient for but leave it for now */
	if (getuid() != 0)
		return;
	pwd = getpwnam("nobody");
	if (pwd == NULL) {
		(void) fprintf(stderr, "failed to get privsep uid\n");
		exit(1);
	}
	if (initgroups("nobody", pwd->pw_gid) == -1) {
		(void) fprintf(stderr, "initgroups failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	if (setgid(pwd->pw_gid) == -1) {
		(void) fprintf(stderr, "setgid failed\n");
		exit(1);
	}
	if (setuid(pwd->pw_uid) == -1) {
		(void) fprintf(stderr, "setuid failed\n");
		exit(1);
	}
}

int
priv_init(struct config_options *clp)
{
	int i, socks[2], cmd;
	struct edge_socks *esp;

	(void) fprintf(stderr, "creating sandbox\n");
	for (i = 1; i < NSIG; i++)
		signal(i, SIG_DFL);
	/* Create sockets */
	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, socks) == -1) {
		(void) fprintf(stderr, "socketpair: %s\n", strerror(errno));
		exit(1);
	}
	child_pid = fork();
	if (child_pid == -1) {
		(void) fprintf(stderr, "fork: %s\n", strerror(errno));
		exit(1);
	}
	if (child_pid == 0) {
		(void) close(socks[0]);
		priv_setuid();
#ifdef __FreeBSD__
		(void) fprintf(stdout, "Entering capability mode sandbox\n");
		if (cap_enter() == -1) {
			(void) fprintf(stderr, "cap_enter failed: %s\n",
			    strerror(errno));
			exit(1);
		}
#endif /* __FreeBSD__ */
#ifdef linux
		(void) fprintf(stdout, "Entering seccomp BPF mode sandbox\n");
		seccomp_activate();
#endif /* linux */
		/* NB: seccomp_bpf, chroot or whatever else
		   Child - drop privileges and return */
		priv_fd = socks[1];
		priv_sep_on = 1;
		return 0;
	}
	/* Pass ALRM/TERM/HUP/INT/QUIT through to child, and accept CHLD */
	signal(SIGALRM, sig_pass_to_chld);
	signal(SIGTERM, sig_pass_to_chld);
	signal(SIGHUP,  sig_pass_to_chld);
	signal(SIGINT,  sig_pass_to_chld);
	signal(SIGQUIT,  sig_pass_to_chld);
	signal(SIGCHLD, sig_chld);
	/* NB: prctl to name process
        setproctitle("[priv]"); */
	close(socks[1]);
	while (!gotsig_chld) {
		if (may_read(socks[0], &cmd, sizeof(int)))
			break;
		switch (cmd) {
		case PRIV_GET_CONF_FD:
			{
			int fd;

			fd = open(clp->config, O_RDONLY);
			if (fd == -1) {
				(void) fprintf(stderr, "config open: %s\n", strerror(errno));
				exit(1);
			}
			send_fd(socks[0], fd);
			close(fd);
			break;
			}
		case PRIV_CONNECT_UNIX:
		case PRIV_BIND_UNIX:
			{
			int sock;
			char sockname[MAX_PATH];

			bzero(sockname, sizeof(sockname));
			must_read(socks[0], sockname, sizeof(sockname) - 1);
			if (cmd == PRIV_BIND_UNIX)
				sock = unix_bind(sockname);
			if (cmd == PRIV_CONNECT_UNIX)
				sock = unix_connect(sockname);
			send_fd(socks[0], sock);
			}
			break;
		case PRIV_GET_CTL_SOCKS:
			esp = edge_setup_sockets(clp);
			if (esp == NULL) {
				(void) fprintf(stderr, "failed to setup control sockets\n");
				exit(1);
			}
			must_write(socks[0], esp, sizeof(*esp));
			for (i = 0; i < esp->nsocks; i++) {
				send_fd(socks[0], esp->socks[i]);
				close(esp->socks[i]);
			}
			free(esp);
			break;
		/*
		 * Add support for our libc highjack shims if required.
		 */
		case PRIV_LIBC_OPEN:
			{
			struct priv_open_args oa;
			int e, fd;

			printf("got open request from surrogate libc\n");
			must_read(socks[0], &oa, sizeof(oa));
			fd = open(oa.pathname, O_RDONLY);
			if (fd == -1) {
				e = errno;
				must_write(socks[0], &e, sizeof(e));
				break;
			}
			e = 0;
			must_write(socks[0], &e, sizeof(e));
			send_fd(socks[0], fd);
			}
			break;
		case PRIV_LIBC_GETADDRINFO:
			{
			struct priv_getaddrinfo_results *ent, *vec;
			struct priv_getaddrinfo_args ga_args;
			struct addrinfo *res, *res0;
			size_t vec_used, vec_alloc;
			size_t curlen;
			int error;

			must_read(socks[0], &ga_args, sizeof(ga_args));
			error = getaddrinfo(ga_args.hostname, ga_args.servname, &ga_args.hints,
			    &res0);
			if (error != 0) {
				printf("getaddrinfo error: %s!\n", gai_strerror(error));
			}
			/* report success/failure */
			must_write(socks[0], &error, sizeof(error));
			if (error != 0)
				break;
			vec_used = 0;
			vec_alloc = 0;
			for (res = res0; res; res = res->ai_next) {
				if (vec == NULL) {
					vec_alloc = sizeof(*ent);
					vec = calloc(1, vec_alloc);
				} else {
					vec_alloc = vec_alloc + sizeof(*ent);
					vec = realloc(vec,vec_alloc);
				}
				ent = &vec[vec_used++];
				ent->ai_flags = res->ai_flags;
				ent->ai_family = res->ai_family;
				ent->ai_socktype = res->ai_socktype;
				ent->ai_protocol = res->ai_protocol;
				ent->ai_addrlen = res->ai_addrlen;
				memcpy(&ent->sas, res->ai_addr, res->ai_addrlen);
				if (res->ai_canonname != NULL) {
					bsd_strlcpy(ent->ai_canonname, res->ai_canonname,
					    sizeof(ent->ai_canonname));
				} else
					ent->ai_canonname[0] = '\0';
			}
			curlen = vec_used * sizeof(*ent);
			if (curlen == 0) {
				curlen = -1;
				must_write(socks[0], &curlen, sizeof(curlen));
				break;
			}
			must_write(socks[0], &curlen, sizeof(curlen));
			must_write(socks[0], vec, curlen);
			}

		default:
			(void) fprintf(stderr, "got request for unknown priv\n");
		}
		/* NB: switch cmd PRIVSEP_GET_CTL_SOCKS */
	}
	_exit(1);
}

/* Read all data or return 1 for error.  */
int
may_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			return (1);
		default:
			pos += res;
		}
	}
	return (0);
}

/* Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh. */
void
must_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/* Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh. */
void
must_write(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = write(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/*
 * Functions to be used by the non-privleged process
 */
struct edge_socks *
priv_get_ctl_socks(void)
{
	struct edge_socks *ep;
	int cmd, k;

	ep = malloc(sizeof(*ep));
	if (ep == NULL)
		return (NULL);
	cmd = PRIV_GET_CTL_SOCKS;
	must_write(priv_fd, &cmd, sizeof(int));
	must_read(priv_fd, ep, sizeof(*ep));
	for (k = 0; k < ep->nsocks; k++) {
		ep->socks[k] = receive_fd(priv_fd);
		printf("got fd %d from privelged process\n", ep->socks[k]);
	}
	return (ep);
}


int
priv_connect_unix(char *name)
{
	char path[MAX_PATH];
	int cmd, s;

	cmd = PRIV_CONNECT_UNIX;
	bzero(path, sizeof(path));
	bsd_strlcpy(path, name, MAX_PATH);
	must_write(priv_fd, &cmd, sizeof(cmd));
	must_write(priv_fd, path, sizeof(path));
	s = receive_fd(priv_fd);
	return (s);
}

int
priv_bind_unix(char *name)
{
	char path[MAX_PATH];
	int cmd, s;

	cmd = PRIV_BIND_UNIX;
	bzero(path, sizeof(path));
	bsd_strlcpy(path, name, MAX_PATH);
	must_write(priv_fd, &cmd, sizeof(cmd));
	must_write(priv_fd, path, sizeof(path));
	s = receive_fd(priv_fd);
	return (s);
}

FILE *
priv_config_open(void)
{
	FILE *fp;
	int cmd, s;

	cmd = PRIV_GET_CONF_FD;
	must_write(priv_fd, &cmd, sizeof(cmd));
	s = receive_fd(priv_fd);
	fp = fdopen(s, "r");
	if (fp == NULL) {
		(void) fprintf(stderr, "fdopen failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	return (fp);
}
