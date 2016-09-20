CC?=	cc
CFLAGS= -Wall -g -fstack-protector -DSECCOMP_AUDIT_ARCH=AUDIT_ARCH_X86_64 -DSECCOMP_FILTER_DEBUG
TARGETS=	edged 
OBJ=	grammar.tab.o lex.yy.o edged.o privsep.o privsep_fdpass.o net.o secbpf.o unix.o util.o
LIBS=	-lpthread

all:	$(TARGETS)

.c.o:
	$(CC) $(CFLAGS) -c $<

lex.yy.o: grammar.tab.o token.l
	flex token.l
	$(CC) $(CFLAGS) -c lex.yy.c

grammar.tab.o: grammar.y
	bison -d grammar.y
	$(CC) $(CFLAGS) -c grammar.tab.c

edged:	$(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -fr *.o $(TARGETS) *.plist *.gcno grammar.tab.c lex.yy.c
