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

#include "edged.h"
#include "net.h"
#include "privsep.h"

void *
edge_accept(void *arg)
{
	struct sockaddr_storage addrstorage;
	int nsock;
	socklen_t len;
	struct listener *ent;

	len = sizeof(struct sockaddr_storage);
	ent = (struct listener *)arg;
	printf("in thread accept on fd %d\n", ent->l_fd);
	while (1) {
		nsock = accept(ent->l_fd, (struct sockaddr *)&addrstorage, &len);
		if (nsock == -1) {
			(void) fprintf(stderr, "accept failed: %s\n",
			    strerror(errno));
			break;
		}
		(void) fprintf(stderr, "accepted connection\n");
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
