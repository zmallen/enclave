#ifndef PRIVSEP_DOT_H_
#define	PRIVSEP_DOT_H_

struct priv_open_args {
        char    pathname[MAXPATHLEN];
        int     flags;
        int     mode;
};

int	priv_init(struct config_options *);
struct edge_socks *
	priv_get_ctl_socks(void);
int	priv_bind_unix(char *);
int	priv_connect_unix(char *);
FILE *	priv_config_open(void);

#endif	/* PRIVSEP_DOT_H_ */
