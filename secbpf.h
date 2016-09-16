#ifndef SECBPF_DOT_H_
#define	SECBPF_DOT_H_

/* Simple helpers to avoid manual errors (but larger BPF programs). */
#define SC_DENY(_nr, _errno) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ERRNO|(_errno))
#define SC_ALLOW(_nr) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)
#define SC_ALLOW_ARG(_nr, _arg_nr, _arg_val) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 4), \
	/* load first syscall argument */ \
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
	    offsetof(struct seccomp_data, args[(_arg_nr)])), \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, (_arg_val), 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW), \
	/* reload syscall number; all rules expect it in accumulator */ \
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
	    offsetof(struct seccomp_data, nr))

void	seccomp_activate(void);

#endif
