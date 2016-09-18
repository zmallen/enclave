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
#include <sys/un.h>

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

int
unix_connect(char *name)
{
	struct sockaddr_un addr;
	int sock;

	sock = socket(PF_UNIX, SOCK_STREAM, PF_UNSPEC);
	if (sock == -1) {
		(void) fprintf(stderr, "UNIX socket failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);
	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		(void) fprintf(stderr, "connect failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	return (sock);
}

int
unix_bind(char *name)
{
	struct sockaddr_un addr;
	int sock;

	(void) unlink(name);
	sock = socket(PF_UNIX, SOCK_STREAM, PF_UNSPEC);
	if (sock == -1) {
		(void) fprintf(stderr, "UNIX socket failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		(void) fprintf(stderr, "failed to bind socket: %s\n",
		    strerror(errno));
		exit(1);
	}
	if (listen(sock, 100) == -1) {
		(void) fprintf(stderr, "failed to listen: %s\n",
		    strerror(errno));
		exit(1);
	}
	return (sock);
}
