
#	DNET SERVERS
#
#
#	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved

NETLIB = ../lib/dnetlib.o
BIN = ../bin


all:	$(NETLIB) $(BIN)/scopy $(BIN)/sshell \
	$(BIN)/sloadav $(BIN)/sgcopy $(BIN)/snfs

$(NETLIB) : ../lib/dnetlib.c
	cc -c ../lib/dnetlib.c
	mv dnetlib.o ../lib

$(BIN)/snfs:		snfs.o
	cc snfs.o $(NETLIB) -o $(BIN)/snfs

$(BIN)/scopy:		scopy.o
	cc scopy.o $(NETLIB) -o $(BIN)/scopy

$(BIN)/sgcopy:		sgcopy.o
	cc sgcopy.o $(NETLIB) -o $(BIN)/sgcopy

$(BIN)/snfs:		snfs.o
	cc snfs.o $(NETLIB) -o $(BIN)/snfs

$(BIN)/sshell:    	sshell.o
	cc sshell.o $(NETLIB) -o $(BIN)/sshell

$(BIN)/sloadav:    	sloadav.o
	cc sloadav.o $(NETLIB) -o $(BIN)/sloadav

clean:
	rm -f *.o make.out

