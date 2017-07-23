
#	THE DNET DRIVER, UNIX END
# 
# 	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
#
#	the BIN should be placed in your path for ease of use.  DNET uses 
#	UNIX level sockets allowing multiple users on a system to run it
#	independantly.  You should make a special directory (Example: .DNET)
#	and then setenv DNETDIR ~/.DNET/  ... all clients and servers will
#	use the enviroment variable to find/create the UNIX level sockets.

NETLIB = ../lib/dnetlib.o

BIN	= ../bin

all:	$(NETLIB) $(BIN)/dnet $

OBJS	= dnet.o control.o tcp.o files.o globals.o subs.o internal.o \
	  dataconv.o packet.o 

$(NETLIB) : ../lib/dnetlib.c
	cc -g -O0 -c ../lib/dnetlib.c
	mv dnetlib.o ../lib

$(BIN)/dnet:	$(OBJS)
	cc -g -O0 $(OBJS) $(NETLIB)  -o $(BIN)/dnet

clean:	
	rm -f *.o make.out

