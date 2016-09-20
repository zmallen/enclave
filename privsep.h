#ifndef PRIVSEP_DOT_H_
#define	PRIVSEP_DOT_H_

int	priv_init(struct cmd_options *);
struct edge_socks *
	priv_get_ctl_socks(void);
int	priv_bind_unix(char *);
int	priv_connect_unix(char *);
FILE *	priv_config_open(void);

#define	PRIV_GET_CTL_SOCKS	1
#define	PRIV_BIND_UNIX		2
#define	PRIV_CONNECT_UNIX	3
#define	PRIV_GET_CONF_FD	4

#endif	/* PRIVSEP_DOT_H_ */
