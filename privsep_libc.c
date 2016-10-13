#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#define _GNU_SOURCE
#define __USE_GNU
#include <dlfcn.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>  /* for offsetof */

#include <privsep_libc.h>

extern int priv_sep_on, priv_fd, child_pid;

static struct addrinfo *
addrinfo_copy(struct priv_getaddrinfo_results *ent)
{
	struct addrinfo *cres;

	cres = malloc(sizeof(*cres));
	if (cres == NULL)
		return (NULL);
	cres->ai_flags = ent->ai_flags;
	cres->ai_family = ent->ai_family;
	cres->ai_socktype = ent->ai_socktype;
	cres->ai_protocol = ent->ai_protocol;
	cres->ai_addrlen = ent->ai_addrlen;
	cres->ai_addr = malloc(cres->ai_addrlen);
	memcpy(cres->ai_addr, &ent->sas, cres->ai_addrlen);
	cres->ai_canonname = strdup(ent->ai_canonname);
	if (cres->ai_canonname == NULL) {
		free(cres);
		return (NULL);
	}
	return (cres);
}

static int
process_getaddr_data(struct priv_getaddrinfo_results *vec, size_t blen,
    struct addrinfo **res)
{
	struct priv_getaddrinfo_results *ent;
	struct addrinfo sentinel, *cres;
	struct addrinfo *head;
	int nitems, k;

	if (blen % sizeof(*ent) != 0)
		return (-1);
	head = NULL;
	nitems = blen / sizeof(*ent);
	for (ent = &vec[0]; ent < &vec[nitems]; ent++) {
		cres = addrinfo_copy(ent);
		if (cres == NULL)
			return (-1);
		cres->ai_next = head;
		head = cres;
	}
	*res = head;
	return (0);
}

int
getaddrinfo(const char *hostname, const char *servname, const struct addrinfo *hints,
    struct addrinfo **res)
{
	int (*o_getaddrinfo)(const char *, const char *, const struct addrinfo *,
	   struct addrinfo **);
	struct priv_getaddrinfo_args ga_args;
	struct priv_getaddrinfo_results *vec;
	size_t blen;
	int cmd, ret;

	if (priv_sep_on == 0) {
		o_getaddrinfo = dlsym(RTLD_NEXT, "getaddrinfo");
		if (o_getaddrinfo == NULL)
			return (-1);
		return (o_getaddrinfo(hostname, servname, hints, res));
	}
	/* NB: locking for threading */
	memset(&ga_args, 0, sizeof(ga_args));
	bsd_strlcpy(&ga_args.hostname, hostname, sizeof(ga_args.hostname));
	bsd_strlcpy(&ga_args.servname, servname, sizeof(ga_args.servname));
	memcpy(&ga_args.hints, hints, sizeof(ga_args.hints));
	cmd = PRIV_LIBC_GETADDRINFO;
	must_write(priv_fd, &cmd, sizeof(cmd));
	must_write(priv_fd, &ga_args, sizeof(ga_args));
	must_read(priv_fd, &ret, sizeof(cmd));
	if (ret != 0)
		return (ret);
	must_read(priv_fd, &blen, sizeof(blen));
	if (blen == -1)
		return (EAI_MEMORY);
	vec = malloc(blen);
	if (vec == NULL)
		return (EAI_MEMORY);
	must_read(priv_fd, vec, blen);
	ret = process_getaddr_data(vec, blen, res);
	free(vec);
	if (ret == -1)
		return (EAI_MEMORY);
	return (0);
}

int
open(const char *path, int flags, ...)
{
	int (*o_open)(const char *, int, ...);
	int mode, cmd, fd, ecode;
	struct priv_open_args oa;
	va_list ap;

	if ((flags & O_CREAT) != 0) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		mode = 0;
	}
	if (priv_sep_on == 0) {
		o_open = dlsym(RTLD_NEXT, "open");
		if (o_open == NULL) {
			return (-1);
		}
		return ((*o_open)(path, flags, mode));
	}
	bsd_strlcpy(oa.pathname, path, MAXPATHLEN);
	oa.mode = mode;
	oa.flags = flags;
	cmd = PRIV_LIBC_OPEN;
	must_write(priv_fd, &cmd, sizeof(cmd));
	must_write(priv_fd, &oa, sizeof(oa));
	must_read(priv_fd, &ecode, sizeof(ecode));
	if (ecode != 0) {
		errno = ecode;
		return (-1);
	}
	fd = receive_fd(priv_fd);
	return (fd);
}
