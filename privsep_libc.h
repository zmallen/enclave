#ifndef PRIVSEP_DOT_H_
#define	PRIVSEP_DOT_H_

struct priv_open_args {
        char    pathname[MAXPATHLEN];
        int     flags;
        int     mode;
};

struct priv_getaddrinfo_args {
	char	hostname[256];
	char	servname[256];
	struct addrinfo hints;
};

struct priv_getaddrinfo_results {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	socklen_t ai_addrlen;
	struct sockaddr_storage sas;
	char ai_canonname[256];
};

enum {
        PRIV_RESERVED,
        PRIV_GET_CTL_SOCKS,
        PRIV_BIND_UNIX,
        PRIV_CONNECT_UNIX,
        PRIV_GET_CONF_FD,
        PRIV_LIBC_OPEN,
	PRIV_LIBC_GETADDRINFO
};

#endif	/* PRIVSEP_DOT_H_ */
