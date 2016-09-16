CC?=	cc
CFLAGS= -Wall -g -fstack-protector 
TARGETS=	edged
OBJ=	edged.o privsep.o privsep_fdpass.o net.o

all:	$(TARGETS)

.c.o:
	$(CC) $(CFLAGS) -c $<

edged:	$(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -fr *.o $(TARGETS)
