
/*
 * FILES.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	handles actions on a per file descriptor basis, including accepting
 *	new connections, closing old connections, and transfering data
 *	between connections.
 */

#include <unistd.h>
#include "dnet.h"
#include "subs.h"

#include "files.h"


/*
 *  new connection over master port... open request.  read two byte port
 *  number, allocate a channel, and send off to the remote
 */

 void do_localopen(long n, long fd) {
    struct sockaddr sa;
    int addrlen = sizeof(sa);
    int s;
    uword chan;

    if (DDebug)
	fprintf(stderr, "DO_LOCALOPEN %ld %ld\n", n, fd);
    while ((s = accept(fd, &sa, &addrlen)) >= 0) {
	chan = alloc_channel();
	fcntl(s, F_SETFL, FNDELAY);
	if (DDebug)
	    fprintf(stderr, " ACCEPT: %ld on channel %ld ", s, chan);
	if (chan == 0xFFFF) {
	    ubyte error = 1;
	    gwrite(s, &error, 1);
	    close(s);
	    if (DDebug)
	        fprintf(stderr, "(no channels)\n");
	    continue;
	} 
	Fdstate[s] = do_open1;
	FdChan[s] = chan;
	FD_SET(s, &Fdread);
	FD_SET(s, &Fdexcept);
	Chan[chan].fd = s;
	Chan[chan].state = CHAN_LOPEN;
	if (DDebug)
	    fprintf(stderr, "(State = CHAN_LOPEN)\n");
    }
}

void do_open1(long n_notused, long fd) {
    uword port;
    char  trxpri[2];
    uword chan = FdChan[fd];
    COPEN co;
    int n;

    if (DDebug)
	fprintf(stderr, "DO_OPEN %ld %ld on channel %ld  ", n, fd, chan);
    for (;;) {
        n = read(fd, &port, 2);
	if (n < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EWOULDBLOCK)
		return;
	}
	read(fd, trxpri, 2);
	if (n != 2)
	    dneterror("do_open1: unable to read 2 bytes");
	break;
    }
    if (DDebug)
	fprintf(stderr, "Port %ld\n", port);
    co.chanh = chan >> 8;
    co.chanl = chan;
    co.porth = port >> 8;
    co.portl = port;
    co.error = 0;
    co.pri   = trxpri[1];
    Chan[chan].port = port;
    Chan[chan].pri = 126;
    WriteStream(SCMD_OPEN, (ubyte*)&co, sizeof(co), chan);
    Chan[chan].pri = trxpri[0];
    Fdstate[fd] = do_openwait;
    if (DDebug)
    	fprintf(stderr, " Newstate = openwait\n");
}

void do_openwait(long n, long fd) {
    ubyte buf[32];
    if (DDebug)
	fprintf(stderr, "************ ERROR DO_OPENWAIT %ld %ld\n", n, fd);
    n = read(fd, &buf, 32);
    if (DDebug) {
	fprintf(stderr, "    OPENWAIT, READ %ld bytes\n", n);
    	if (n < 0)
	    perror("openwait:read");
    }
}

void do_open(long nn, long fd) {
    extern void nop();
    char buf[256];
    uword chan = FdChan[fd];
    int n;

    n = read(fd, buf, sizeof(buf));
    if (DDebug) {
	fprintf(stderr, "DO_OPEN %ld %ld, RECEIVE DATA on chan %ld (%ld by)\n",
	    nn, fd, chan, n);
	fprintf(stderr, " fd, chanfd %ld %ld\n", fd, Chan[chan].fd);
	if (n < 0)
	    perror("open:read");
    }
    if (n == 0 || nn == 2) {	/* application closed / exception cond */
	CCLOSE cc;

	if (DDebug)
	    fprintf(stderr, " DO_OPEN: REMOTE EOF, channel %d\n", chan);

	cc.chanh = chan >> 8;
	cc.chanl = chan;
	WriteStream(SCMD_CLOSE, &cc, sizeof(CCLOSE), chan);
	Chan[chan].state = CHAN_CLOSE;
	Chan[chan].flags |= CHANF_LCLOSE;
	if (Chan[chan].flags & CHANF_RCLOSE) {
	    ;
	    /* should never happen
	    int fd = Chan[chan].fd;
	    Chan[chan].state = CHAN_FREE;
	    Chan[chan].fd = -1;
	    Fdstate[fd] = nop;
	    FD_CLR(fd, &Fdread);
	    FD_CLR(fd, &Fdexcept);
	    close(fd);
	    */
	} else {
	    FD_CLR(fd, &Fdread);
	    FD_CLR(fd, &Fdexcept);
	}
    }
    if (n > 0) {
	WriteStream(SCMD_DATA, (ubyte *) &buf, n, chan);
    }
}

