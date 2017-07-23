
/*
 *  CONTROL.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *  -handle various actions:
 *	RTO	- read timeout
 *	WTO	- write timeout (a packet not acked)
 *	RNET	- state machine for raw receive packet
 *	WNET	- starts timeout sequence if Write occured
 *	WUPDATE - update write windows, send packets
 *	RUPDATE - execute received packets in correct sequence
 *	RECCMD	- execute decoded SCMD commands obtained from received
 *			packets.
 */

#include <stdio.h>
#include <unistd.h>

#include "dnet.h"
#include "subs.h"
#include "internal.h"

#include "packet.h"

#include "control.h"

ubyte TmpBuf[MAXPACKET];

/*
 *  RTO:    read timeout.   Timeout occured while waiting for the rest of
 *	    a packet.  Reset state and restart read.
 */

void do_rto(void) {
    RState = 0;
    RcvData = 0;
    if (DDebug)
	fprintf(stderr, "RTO\n");
}

/*
 *  WTO:    Write timeout (unresolved output windows exist).  Resend a CHECK
 *	    command for all unresolved (READY) output windows
 */

void do_wto(void) {
    register short i;
    register PKT *pkt;
    short to = 0;

    if (DDebug)
	fprintf(stderr, "WTO\n");
    if (Restart) {
	WriteRestart();
	return;
    }
    for (i = 0; i < WPUsed; ++i) {
	pkt = WPak[i];
	if (pkt->state == READY) {  /*  send check  */
	    WriteChk((WPStart + i) & 7);
	    ++to;
	}
    }
    if (to)
	WTimeout(WTIME);
}

/*
 *  RNET:   Receved data ready from network.  The RNet request will
 *	    automagically be reissued on return.
 */

void do_rnet(void) {
    ubyte *ptr = RcvBuf;
    long len = RcvData;

    RcvData = 0;

    RExpect = RecvPacket(ptr, len);
}


/*
 *  DO_WUPDATE()
 *
 *  (1) Remove EMPTY windows at head of transmit queue.
 *  (2) Fill up transmit queue with pending requests, if any.
 *
 *  First two bits determine CMD as follows:
 *	    0bbbbbbb+0x20	DATA	    0-95 bytes of DATA
 *	    10bbbccc		DATA	    0-7 bytes of DATA, ccc=channel
 *								  command.
 *	    11bbbbbb bbbbbbbb	DATA	    0-1023 bytes of DATA
 *
 *  Note!  By encoding the quick-data packet with the length + 0x20, you
 *  don't take the 6 bit encoding hit when sending small amounts of ascii
 *  data such as keystrokes.
 */

int  do_wupdate(void) {
    static short XPri;
    int maxpktsize;
    register XIOR *ior;
    register PKT *pkt;
    register long len;
    short loop = 0;

    WReady = 0;
    while (WPUsed && WPak[0]->state == EMPTY) {
	pkt = WPak[0];
	WPak[0] = WPak[1];
	WPak[1] = WPak[2];
	WPak[2] = WPak[3];
	WPak[3] = pkt;
	--WPUsed;
	++WPStart;
    }
    if (Restart)
	return(0);

    while (WPUsed != 4 && (ior = (XIOR *)RemHead(&TxList))) {
	register long offset = 0;

	if (++loop == 2) {
	    WReady = 1;
	    AddHead(&TxList, ior);
	    return(0);
	}

	{
	    short npri;

	    if (ior->io_Command == SCMD_DATA)
		npri = ior->io_Pri << 2;
	    else
		npri = XPri;
	    if (npri >= XPri)  {
		XPri = npri;
	    } else {
		if (XPri - npri > 100)
		    XPri -= 10;
		else if (XPri - npri > 50)
		    XPri -= 5;
		else
		    --XPri;
	    }
	    maxpktsize = MAXPKT - (XPri - npri);
	    if (DDebug)
		fprintf(stderr, "**MAXPKTSIZE = %ld  XPri %ld npri %ld\n",
		    maxpktsize, XPri, npri
		);
	    if (maxpktsize < MINPKT)
		maxpktsize = MINPKT;
	}

	pkt = WPak[WPUsed];

	for (;;) {
	    if (offset > (maxpktsize-8))            /*  not enough room */
		break;
	    if (ior->io_Command == SCMD_DATA && ior->io_Channel != WChan) {
		/*  CSWITCH */
		WChan = ior->io_Channel;
		TmpBuf[offset+0] = 0x80|SCMD_SWITCH|(2<<3);
		TmpBuf[offset+1] = WChan >> 8;
		TmpBuf[offset+2] = WChan;
		offset += 3;
		if (DDebug)
		    fprintf(stderr, "SEND SWITCH %ld\n", WChan);
	    }
	    len = ior->io_Length - ior->io_Actual;
	    if (ior->io_Command == SCMD_DATA) {     /*  DATA    */
		if (DDebug)
		    fprintf(stderr, "SEND DATA %ld bytes\n", len);
		if (offset + len > (maxpktsize-4))
		    len = (maxpktsize-4) - offset;
		if (len < 0x20) {
		    TmpBuf[offset] = len + 0x20;
		    ++offset;
		} else {
		    TmpBuf[offset+0] = 0x40 + len / 96;
		    TmpBuf[offset+1] = 0x20 + len % 96;
		    offset += 2;
		}
	    } else {				    /*	COMMAND */
		TmpBuf[offset] = 0x80|ior->io_Command|(len<<3);
		++offset;
		if (DDebug)
		    printf("constr sdcmd %02x len %d\n", TmpBuf[0], len);
	    }
	    bcopy((char *)ior->io_Data+ior->io_Actual,TmpBuf+offset,len);
	    offset += len;
	    ior->io_Actual += len;
	    if (ior->io_Actual == ior->io_Length) {
		free(ior->io_Data);
		free(ior);
		ior = (XIOR *)RemHead(&TxList);          /* Next packet      */
		if (ior == NULL)
		    break;
	    }
	}
	pkt->state = READY;
	BuildDataPacket(pkt, (WPStart + WPUsed) & 7, TmpBuf, offset);
	WritePacket(pkt);

	if (ior) {
	    ++ior->io_Pri;
	    Enqueue(&TxList, ior);
	    --ior->io_Pri;
	}
	++WPUsed;
    }
}

void dumpcheck(register ubyte *ptr) {
    register short i;
    for (i = 0;i < 8; ++i) {
	if (ptr[i])
	    replywindow(i);
	ptr[i] = 0;
    }
}

void do_cmd(short ctl, ubyte *buf, int bytes) {
    ubyte window = ctl & 7;
    ubyte rwindow;
    static ubyte Chk, Chkwin[8];	/* remember window checks */

    if (ctl == -1)  {                           /* end of sequence */
	dumpcheck(Chkwin);
	Chk = 0;
	return;
    }
    if ((ctl & PKF_MASK) == PKCMD_CHECK) {      /* CHECK packet    */
	Chkwin[window] = 1;
	Chk = 1;
	return;
    }
    if (Chk) {                                  /* NON-CHECK packet*/
	dumpcheck(Chkwin);
	Chk = 0;
    }
    switch(ctl & PKF_MASK) {
    case PKCMD_WRITE:
    case PKCMD_WRITE6:
    case PKCMD_WRITE7:
	rwindow = (window - RPStart) & 7;
	if (rwindow < 4) {
	    bcopy(buf, RPak[rwindow]->data, bytes);
	    RPak[rwindow]->buflen = bytes;
	    RPak[rwindow]->state = READY;
	    if (rwindow == 0)
		do_rupdate();
	}
	replywindow(window);
	break;
    case PKCMD_ACK:
	rwindow = (window - WPStart) & 7;
	if (rwindow < WPUsed)       /*  mark as sent    */
	    WPak[rwindow]->state = EMPTY;
	break;
    case PKCMD_NAK:		    /*	resend		*/
	rwindow = (window - WPStart) & 7;
	if (rwindow < WPUsed) {     /*  resend          */
	    WritePacket(WPak[rwindow]);
	} else {
	    fprintf(stderr, "Soft Error: Illegal NAK\n");
	}
	break;
    case PKCMD_ACKRSTART:
    case PKCMD_RESTART:
	{
	    uword chan;
	    uword chksum;
	    int len;
	    int fd;

	    if ((ctl & PKF_MASK) == PKCMD_ACKRSTART) {
			Restart = 0;
		}
	    do_netreset();
		if (DDebug) {
			fprintf(stderr, "after do_netreset");
		}
	    if ((ctl & PKF_MASK) == PKCMD_RESTART) {
			WritePacket(BuildRestartAckPacket("3", 1));
	    }
		if (DDebug) {
			fprintf(stderr, "after WritePacket");
		}
	    if (bytes) {
			setlistenport(buf);
		} else {
			setlistenport("3");
		}
	    do_wupdate();
		if (DDebug) {
			fprintf(stderr, "after do_wupdate");
		}
	}
	break;
    default:
	if (DDebug)
	    printf("do_cmd: bad packet ctl %02x\n", ctl);
	break;
    }
    do_rupdate();
			if (DDebug) {
			fprintf(stderr, "after do_rupdate");
		}
}

void do_rupdate(void) {
    while (RPak[0]->state == READY) {
	register PKT *pkt = RPak[0];
	register ubyte *ptr = pkt->data;
	register uword len;
	uword iolen = pkt->buflen;
	ubyte cmd;

	while (iolen) {
	    if (DDebug)
	        printf("rupdate %02x\n", ptr[0]);
	    cmd = SCMD_DATA;
	    len = ptr[0];
	    ++ptr;
	    --iolen;
	    if (len >= 128) {
		cmd = len & 7;
		len = (len >> 3) & 7;
	    } else {
		if (len < 0x40) {
		    len -= 0x20;
		} else {
		    len = (len - 0x40) * 96 + (*ptr - 0x20);
		    ++ptr;
		    --iolen;
		}
	    } 
	    iolen -= len;
	    if (DDebug)
		fprintf(stderr, " MPXCMD %ld (%ld bytes)\n", cmd, len);
	    do_reccmd(cmd, ptr, len);
	    ptr += len;
	}
	RPak[0] = RPak[1];
	RPak[1] = RPak[2];
	RPak[2] = RPak[3];
	RPak[3] = pkt;
	pkt->state = EMPTY;
	++RPStart;
    }
}

void do_reccmd(ubyte cmd, ubyte *ptr, int len) {
    if (DDebug)
	printf("-reccmd %02x (%ld bytes)\n", cmd, len);
    switch(cmd) {
    case SCMD_DATA:
	if (RChan < MAXCHAN && (Chan[RChan].flags & CHANF_ROK))
	    gwrite(Chan[RChan].fd, ptr, len);
	break;
    case SCMD_SWITCH:
	RChan = (ptr[0]<<8)|ptr[1];
	break;
    case SCMD_OPEN:
	{
	    register COPEN *cop = (COPEN *)ptr;
	    CACKCMD ack;
	    uword chan = (cop->chanh << 8) | cop->chanl;

	    ack.chanh = cop->chanh;
	    ack.chanl = cop->chanl;
	    ack.error = 0;

	    if (chan >= MAXCHAN || Chan[chan].state) {
		ack.error = 33;     /* magic */
		WriteStream(SCMD_ACKCMD, &ack, sizeof(CACKCMD), chan);
		break;
	    }
	    {
		int error;
		int s;
		uword port = (cop->porth<<8)|cop->portl;

		if (isinternalport(port)) {
		    error = iconnect(&s, port);
		} else {
		    struct sockaddr sa;
		    s = socket(PF_UNIX, SOCK_STREAM, 0);
		    if (DDebug)
			fprintf(stderr, " REC OPEN, CONNECTING ch%d po%d\n",
			    chan, port
			);
		    sa.sa_family = AF_INET;
		    sprintf(sa.sa_data,".PORT.%ld", port);
		    error = connect(s, &sa, sizeof(sa));
		    if (error < 0) {
			startserver(port);
			error = connect(s, &sa, sizeof(sa));
		    }
		    if (DDebug)
			fprintf(stderr, " CONNECTED err=%ld\n", error);
		}
		if (error < 0) {
		    ack.error = 2;
		} else {
		    extern void do_open();
		    fcntl(s, F_SETFL, FNDELAY);
		    Chan[chan].state = CHAN_OPEN;
		    Chan[chan].flags = CHANF_ROK|CHANF_WOK;
		    Chan[chan].fd = s;
		    Chan[chan].pri= cop->pri;
		    FD_SET(s, &Fdread);
		    FD_SET(s, &Fdexcept);
		    FdChan[s] = chan;
		    Fdstate[s] = do_open;
		}
		WriteStream(SCMD_ACKCMD, &ack, sizeof(CACKCMD), -1);
	    }
	}
	break;
    case SCMD_CLOSE:	/*  receive close   */
	{
	    extern void nop();
	    register CCLOSE *clo = (CCLOSE *)ptr;
	    uword chan = (clo->chanh<<8)|clo->chanl;
	    int fd = Chan[chan].fd;

	    if (DDebug)
		fprintf(stderr, " SCMD_CLOSE\n");
	    if (chan >= MAXCHAN || Chan[chan].state == CHAN_FREE)
		break;
	    /*
	    Chan[chan].state = CHAN_CLOSE;
	    Chan[chan].flags |= CHANF_RCLOSE;
	    */
	    Chan[chan].flags &= ~(CHANF_ROK|CHANF_WOK);
	    FD_CLR(fd, &Fdread);
	    FD_CLR(fd, &Fdexcept);
	    Chan[chan].state = CHAN_FREE;
	    Chan[chan].fd = -1;
	    Fdstate[fd] = nop;
	    close(fd);
	    ClearChan(&TxList, chan, 0);

	    if (Chan[chan].flags & CHANF_LCLOSE) {
		if (DDebug)
		    fprintf(stderr," REMOTE CLOSE %ld, LOCAL ALREADY CLOSED\n",
			fd
		    );
	    } else {
		CCLOSE cc;
		char dummy;
		cc.chanh = chan >> 8;
		cc.chanl = chan;
		WriteStream(SCMD_CLOSE, &cc, sizeof(CCLOSE), chan);
		/*
		shutdown(Chan[chan].fd, 2);
		write(Chan[chan].fd, &dummy, 0);
		*/
		if (DDebug)
		    fprintf(stderr," REMOTE CLOSE %ld, LOCAL NOT YET CLOSED\n",
			fd
		    );
	    }
	}
	break;
    case SCMD_ACKCMD:	/*  acknowledge of my open	*/
	{
	    register CACKCMD *cack = (CACKCMD *)ptr;
	    uword chan = (cack->chanh<<8)|cack->chanl;
	    if (chan >= MAXCHAN || Chan[chan].state != CHAN_LOPEN)
		break;
	    if (DDebug)
		fprintf(stderr, "ackerr = %ld\n", cack->error);
	    if (cack->error == 33) {
		uword newchan = alloc_channel();
		COPEN co;
		if (newchan < MAXCHAN) {
		    Chan[newchan] = Chan[chan];
		    Chan[chan].state = CHAN_FREE;
		    Chan[chan].fd = -1;
		    co.chanh = newchan >> 8;
		    co.chanl = newchan;
		    co.porth = Chan[chan].port >> 8;
		    co.portl = Chan[chan].port;
		    co.error = 0;
		    co.pri   = Chan[chan].pri;
		    WriteStream(SCMD_OPEN, &co, sizeof(COPEN), newchan);
		    break;
		}
	    }
	    if (cack->error) {
		extern void nop();
		ubyte error = cack->error;
		int fd = Chan[chan].fd;

		gwrite(fd, &error, 1);
		Fdstate[fd] = nop;
		Chan[chan].fd = -1;
		Chan[chan].state = CHAN_FREE;
		FD_CLR(fd, &Fdread);
		FD_CLR(fd, &Fdexcept);
		close(fd);
	    } else {
		ubyte error = 0;
		extern void do_open();
		gwrite(Chan[chan].fd, &error, 1);
		Chan[chan].state = CHAN_OPEN;
		Chan[chan].flags = CHANF_ROK|CHANF_WOK;
		Fdstate[Chan[chan].fd] = do_open;
	    }
	}
	break;
    case SCMD_EOFCMD:	/*  EOF on channel		*/
	{
	    register CEOFCMD *eof = (CEOFCMD *)ptr;
	    uword chan = (eof->chanh<<8)|eof->chanl;

	    if (chan < MAXCHAN && Chan[chan].state == CHAN_OPEN) {
		Chan[chan].flags &= ~eof->flags;
		if (eof->flags & CHANF_ROK) {
		    char dummy;
		    shutdown(Chan[chan].fd, 1);
		    write(Chan[chan].fd, &dummy, 0);
		}
	    }
	}
	break;
    case SCMD_QUIT:
	dneterror("QUIT");
	break;
    case SCMD_IOCTL:
	{
	    register CIOCTL *cio = (CIOCTL *)ptr;
	    uword chan = (cio->chanh<<8)|cio->chanl;

	    if (chan < MAXCHAN && Chan[chan].state == CHAN_OPEN) {
		switch(cio->cmd) {
		case CIO_SETROWS:
		    isetrows(Chan[chan].fd, (cio->valh<<8)|cio->vall);
		    break;
		case CIO_SETCOLS:
		    isetcols(Chan[chan].fd, (cio->valh<<8)|cio->vall);
		    break;
		case CIO_STOP:
		    break;
		case CIO_START:
		    break;
		case CIO_FLUSH:
		    ClearChan(&TxList, chan, 0);
		    break;
		}
	    }
	}
	break;
    default:
	break;
    }
}

void replywindow(ubyte window) {
    ubyte rwindow = (window - RPStart) & 7;

    if (DDebug) {
	if (rwindow >= 4 || RPak[rwindow]->state == READY)
	    fprintf(stderr, " ACK WINDOW %ld\n", window);
	else
	    fprintf(stderr, " NAK WINDOW %ld\n", window);
    }
    if (rwindow >= 4 || RPak[rwindow]->state == READY)  /* data ready */
	WriteAck(window);
    else
	WriteNak(window);
}

