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
#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <regex.h>

#include "edged.h"
#include "net.h"
#include "privsep.h"
#include "privsep_fdpass.h"
#include "util.h"

int
edge_parse_line(regex_t *rp, char *line, char **k, char **v)
{
	regmatch_t mgroups[10];
	size_t len;
	char *copy;

	if (regexec(rp, line, 10, mgroups, 0)) {
		*k = *v = NULL;
		return (-1);
	}
	if (mgroups[3].rm_so != -1) {
		*k = *v = NULL;
		return (-1);
	}
	len = strlen(line) + 1;
	copy = calloc(1, len);
	if (copy == NULL) {
		(void) fprintf(stderr, "calloc failed\n");
		*k = *v = NULL;
		return (-1);
	}
	bsd_strlcpy(copy, line, len -1);
	copy[mgroups[1].rm_eo] = '\0';
	*k = strdup(copy + mgroups[1].rm_so);
	bzero(copy, len);
	bsd_strlcpy(copy, line, len - 1);
	copy[mgroups[2].rm_eo] = '\0';
	*v = strdup(copy + mgroups[2].rm_so);
	free(copy);
	return (0);
}

int
edge_peek_hosthdr(int sock, regex_t *rp)
{
	char hbuf[2048], *state, *k, *v, *ptr;
	ssize_t cc;

	while (1) {
		/* NB: Host: header needs to be in the first 2048 bytes
		 * we need to fix this if we want to do more than POC
		 */
		bzero(hbuf, sizeof(hbuf));
		cc = recv(sock, hbuf, sizeof(hbuf) - 1, MSG_PEEK);
		if (cc == 0)
			return (0);
		if (cc == -1 && errno == EINTR)
			continue;
		state = &hbuf[0];
		while ((ptr = strtok_r(state, "\n", &state))) {
			edge_parse_line(rp, ptr, &k, &v);
			if (k == NULL || v == NULL)
				continue;
			free(k);
			free(v);
		}
		break;
	}
	return (0);
}

void *
edge_accept(void *arg)
{
	struct sockaddr_storage addrs;
	struct listener *ent;
	int nsock, fam;
	socklen_t len;
	regex_t rd;

	ent = (struct listener *)arg;
	len = sizeof(int);
	if (getsockopt(ent->l_fd, SOL_SOCKET, SO_DOMAIN, &fam, &len) == 1) {
		(void) fprintf(stderr, "getsockopt failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	switch (fam) {
	case PF_INET:
		len = sizeof(struct sockaddr_in);
		break;
	case PF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
	default:
		(void) fprintf(stderr, "un-supported address family\n");
		exit(1);
	}
	printf("in thread accept on fd %d\n", ent->l_fd);
        if (regcomp(&rd, "^(.*): (.*)", REG_EXTENDED)) {
                (void) fprintf(stderr, "failed to compile re\n");
                return (NULL);
        }
	while (1) {
		nsock = accept(ent->l_fd, (struct sockaddr *)&addrs, &len);
		if (nsock == -1) {
			(void) fprintf(stderr, "accept failed: %s\n",
			    strerror(errno));
			break;
		}
		(void) fprintf(stderr, "accepted connection\n");
		edge_peek_hosthdr(nsock, &rd);
		(void) close(nsock);
	}
	return (NULL);
}

void
edge_cleanup_socks(struct edge_socks *es)
{
	int k;

	for (k = 0; k < es->nsocks; k++)
		(void) close(es->socks[k]);
}

struct edge_socks *
edge_setup_sockets(struct cmd_options *cmd)
{
	struct addrinfo hints, *res, *res0;
	struct edge_socks *es;
	int error, s, o;

	bzero(&hints, sizeof(hints));
	hints.ai_family = cmd->family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(cmd->src, cmd->port, &hints, &res0);
	if (error) {
		(void) fprintf(stderr, "getaddrinfo failed: %s\n",
		    gai_strerror(error));
		return (NULL);
	}
	es = malloc(sizeof(*es));
	if (es == NULL) {
		freeaddrinfo(res0);
		return (NULL);
	}
	bzero(es, sizeof(*es));
	o = 1;
	error = 0;
	for (res = res0; res != NULL && es->nsocks < MAXSOCKS;
	    res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			(void) fprintf(stderr, "socket: %s\n", strerror(errno));
			error = 1;
			break;
		}
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) == -1) {
			(void) fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n",
			    strerror(errno));
			error = 1;
			break;
		}
		if (bind(s, res->ai_addr, res->ai_addrlen) == -1) {
			(void) fprintf(stderr, "bind: %s\n", strerror(errno));
			error = 1;
			break;
		}
		if (listen(s, 128) == -1) {
			(void) fprintf(stderr, "listen: %s\n", strerror(errno));
			error = 1;
			break;
		}
		es->socks[es->nsocks++] = s;
		printf("setup socket fd %d\n", s);
	}
	if (error) {
		edge_cleanup_socks(es);
		free(es);
		es = NULL;
	}
	freeaddrinfo(res0);
	return (es);
}
