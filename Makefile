CC?=	cc
CFLAGS= -Wall -g -fstack-protector -DSECCOMP_AUDIT_ARCH=AUDIT_ARCH_X86_64 -DSECCOMP_FILTER_DEBUG
TARGETS=	edged 
OBJ=	grammar.tab.o lex.yy.o edged.o privsep.o privsep_fdpass.o net.o secbpf.o unix.o util.o
LIBS=	-lpthread

all:	$(TARGETS) privsep_libc.o libbad.so

.c.o:
	$(CC) $(CFLAGS) -c $<

lex.yy.o: grammar.tab.o token.l
	flex token.l
	$(CC) $(CFLAGS) -c lex.yy.c

grammar.tab.o: grammar.y
	bison -d grammar.y
	$(CC) $(CFLAGS) -c grammar.tab.c

libbad.so:
	$(CC) -fpic -c libbad.c
	$(CC) -shared -o libbad.so libbad.o

privsep_libc.o:
	$(CC) -fPIC -shared -o privsep_libc.so privsep_libc.c -ldl -I.

edged:	$(OBJ) libbad.so
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS) -Wl,-E -lbad -L.

clean:
	rm -fr *.o $(TARGETS) *.plist *.so *.gcno grammar.tab.c grammar.tab.h lex.yy.c
