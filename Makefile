CC?=	cc
CFLAGS= -Wall -g -fstack-protector 
TARGETS=	edged
OBJ=	edged.o privsep.o privsep_fdpass.o net.o
LIBS=	-lpthread

all:	$(TARGETS)

.c.o:
	$(CC) $(CFLAGS) -c $<

edged:	$(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -fr *.o $(TARGETS)
