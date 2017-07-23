
/*
 *  CHANNEL.H
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *  Channel structures for SCMD_* channel commands.
 *
 *  NOTE: These structures may be ONLY 0,2,4, or 6 bytes in size.
 */

#define CSWITCH struct _CSWITCH
#define COPEN	struct _COPEN
#define CCLOSE	struct _CCLOSE
#define CACKCMD struct _CACKCMD
#define CEOFCMD struct _CEOFCMD
#define CIOCTL  struct _CIOCTL

CSWITCH {		/*  SWITCH current data channel */
    ubyte   chanh;	/*  channel to switch to	*/
    ubyte   chanl;
};

COPEN { 		/*  OPEN port on channel	*/
    ubyte   chanh;	/*  try to use this channel     */
    ubyte   chanl;
    ubyte   porth;	/*  requested port from client  */
    ubyte   portl;
    ubyte   error;	/*  error return 0=ok		*/
    char    pri;	/*  chan. priority -127 to 126  */
};

CCLOSE {		/*  CLOSE a channel		*/
    ubyte   chanh;	/*  channel to close		*/
    ubyte   chanl;
};

CACKCMD {		/*  Acknowledge an open	    	*/
    ubyte   chanh;	/*  channel   			*/
    ubyte   chanl;
    ubyte   error;	/*  0=ok 1=fail 33=retry w/ different channel	*/
    ubyte   filler;
};

CEOFCMD {		/*  Send [R/W] EOF		*/
    ubyte   chanh;	/*  channel to send EOF on	*/
    ubyte   chanl;
    ubyte   flags;	/*  CHANF_ROK/WOK (bits to clear)  */
    ubyte   filler;
};

CIOCTL {
    ubyte   chanh;	/* channel			*/
    ubyte   chanl;
    ubyte   cmd;	/* ioctl command		*/
    ubyte   valh;	/* ioctl value			*/
    ubyte   vall;
    ubyte   valaux;	/* auxillary field		*/
};

#define CIO_SETROWS	1	/* PTY's only			*/
#define CIO_SETCOLS	2	/* PTY's only			*/
#define CIO_STOP	3	/* any channel, flow control	*/
#define CIO_START	4	/* any channel, flow control	*/
#define CIO_FLUSH	5	/* any channel, flush pending 	*/  
				/* writes down the toilet	*/

#define CHAN	struct _CHAN

CHAN {
    int	    fd;		/*	file descriptor for channel		*/
    uword   port;	/*	port#  (used in open sequence)		*/
    ubyte   state;	/*	state of channel			*/
    ubyte   flags;	/*	channel flags				*/
    char    pri;	/*	priority of channel			*/
};

