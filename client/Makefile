
#	DNET CLIENTS
#
#   DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved.
#

NETLIB = ../lib/dnetlib.o
BIN = ../bin


all:	$(NETLIB)  $(BIN)/dsoc $(BIN)/putfiles $(BIN)/dprint \
	$(BIN)/getfiles

#$(BIN)/draw:		draw.o
#	cc draw.o $(NETLIB) -o $(BIN)/draw

$(BIN)/dsoc:		dsoc.o
	cc dsoc.o $(NETLIB) -o $(BIN)/dsoc

$(BIN)/getfiles:    	getfiles.o 
	cc getfiles.o $(NETLIB) -o $(BIN)/getfiles

$(BIN)/putfiles:    	putfiles.o 
	cc putfiles.o $(NETLIB) -o $(BIN)/putfiles

$(BIN)/dprint:		dprint.o
	cc dprint.o $(NETLIB) -o $(BIN)/dprint

clean:
	rm -f *.o make.out

