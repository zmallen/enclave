#ifndef PRIVSEP_DOT_H_
#define	PRIVSEP_DOT_H_

int	priv_init(struct cmd_options *);
struct edge_socks *
	priv_get_ctl_socks(void);

#define	PRIV_GET_CTL_SOCKS	1

#endif	/* PRIVSEP_DOT_H_ */
