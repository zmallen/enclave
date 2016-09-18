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
#include <pthread.h>

#include "edged.h"
#include "net.h"
#include "privsep.h"
#include "privsep_fdpass.h"

static struct listener *head, *tail;

struct listener *
add_listener(int fd)
{
	struct listener *ent;

	ent = calloc(1, sizeof(*ent));
	if (ent == NULL) {
		(void) fprintf(stderr, "failed to allocate\n");
		exit(1);
	}
	ent->l_fd = fd;
	ent->l_next = NULL;
	if (head == NULL)
		head = ent;
	else
		tail->l_next = ent;
	tail = ent;
	return (ent);
}

void
do_cached(struct cmd_options *cmd)
{
	int sock, cs;
	int osock;
	char buf[256];

	sock = priv_bind_unix(cmd->name);
	while (1) {
		cs = accept(sock, NULL, NULL);
		if (cs == -1) {
			(void) fprintf(stderr, "error in accept\n");
			exit(1);
		}
		printf("accepted connection\n");
		osock = receive_fd(cs);
		printf("got fd %d\n", osock);
		while (1) {
			size_t cc;

			cc = recv(osock, buf, sizeof(buf), 0);
			printf("read %zu bytes\n", cc);
			printf("%s\n", buf);
			if (cc == 0)
				break;
		}
		(void) close(cs);
	}
	return;
}

int
main(int argc, char *argv [])
{
	struct edge_socks *es;
	struct cmd_options cmd;
	struct listener *ent;
	int ch, j;

	cmd.family = PF_UNSPEC;
	cmd.src = NULL;
	cmd.port = "http";
	while ((ch = getopt(argc, argv, "46n:p:s:")) != -1)
		switch (ch) {
		case '4':
			cmd.family = PF_INET;
			break;
		case '6':
			cmd.family = PF_INET6;
			break;
		case 'n':
			cmd.name = optarg;
			break;
		case 's':
			cmd.src = optarg;
			break;
		case 'p':
			cmd.port = optarg;
			break;
		}
	priv_init(&cmd);
	if (cmd.name != NULL) {
		do_cached(&cmd);
		return (0);
	}
	es = priv_get_ctl_socks();
	if (es == NULL) {
		(void) fprintf(stderr, "error setting up control socks\n");
		return (-1);
	}
	for (j = 0; j < es->nsocks; j++) {
		ent = add_listener(es->socks[j]);
		if (pthread_create(&ent->l_thr, NULL, edge_accept, ent) != 0) {
			(void) fprintf(stderr, "failed to launch thread\n");
			return (-1);
		}
		(void) fprintf(stdout, "launched thread for fd %d\n",
		    es->socks[j]);
	}
	while (head != NULL) {
		pthread_join(head->l_thr, &head->l_ret);
		head = head->l_next;
	}
	return (0);
}
