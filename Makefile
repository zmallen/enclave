CC?=	cc
CFLAGS= -Wall -g -fstack-protector -DSECCOMP_AUDIT_ARCH=AUDIT_ARCH_X86_64 -DSECCOMP_FILTER_DEBUG
TARGETS=	edged
OBJ=	edged.o privsep.o privsep_fdpass.o net.o secbpf.o
LIBS=	-lpthread

all:	$(TARGETS)

.c.o:
	$(CC) $(CFLAGS) -c $<

edged:	$(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -fr *.o $(TARGETS)
