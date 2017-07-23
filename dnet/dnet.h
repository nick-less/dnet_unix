#ifndef DNET_H
#define DNET_H
/*
 *  DNET.H
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/file.h>

/* V2.01 */
#include <sys/param.h>
#include <pwd.h>

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#ifndef FD_SETSIZE
#define FD_SETSIZE (sizeof(struct fd_set) * 8)
#endif
#ifndef NFDBITS
#define NFDBITS 32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif
#ifndef sigmask
#define sigmask(m) (1 << ((m)-1))
#endif

#ifndef LASTTRYDNETSERVERS
#define LASTTRYDNETSERVERS "/usr/local/lib/dnet/dnet.servers"
#endif

struct Node {
    struct Node *ln_Succ;
    struct Node *ln_Pred;
};

struct List {
    struct Node *lh_Head;
    struct Node *lh_Tail;
    struct Node *lh_TailPred;
};


typedef unsigned char	ubyte;
typedef unsigned short	uword;
typedef unsigned long	ulong;

typedef struct List	    LIST;
typedef struct Node	    NODE;

#include "channel.h"

#define PKT struct _PKT
#define CTLPKT struct _CTLPKT
#define XIOR struct _XIOR

#define EMPTY	0	/*  empty (sent)                    */
#define READY	1	/*  data ready (not sent yet)       */
#define RCVBUF  4096

#define MAXCHAN 128	/*  Max # of channels supported     */
#define SYNC	0x5B	/*  SYNC character		    */
#define WTIME	8 	/*  in seconds (expect ack)   		*/
#define RTIME	4	/*  in seconds (read to)      		*/
#define MAXPKT	200	/*  maximum packet size  (data area)	          */
#define MINPKT  32	/*  minimum packet size  (data area) for purposes */
			/*  of determining the dynamic maximum packet size*/
			/*  only.  Actual minimum is, of course, 1 byte   */

#define MAXPACKET ((MAXPKT * 8 + 5) / 6 + 64)

#define OVERHEAD    7	/*  for packets with data	    */

XIOR {
    NODE    io_Node;
    ubyte   *io_Data;
    ulong   io_Length;
    ulong   io_Actual;
    uword   io_Channel;
    ubyte   io_Command;
    ubyte   io_Error;
    char    io_Pri;
};

PKT {
    uword   buflen;
    ubyte   state;	/*  EMPTY, READY				    */

    ubyte   sync;	/*  THE PACKET	    */
    ubyte   ctl;
    ubyte   cchk;
    ubyte   lenh;
    ubyte   lenl;
    ubyte   dchkh;
    ubyte   dchkl;
    ubyte   data[MAXPACKET];
};

CTLPKT {
    uword buflen;
    ubyte state;

    ubyte sync;
    ubyte ctl;
    ubyte cchk;
};

	/*
	 *    In receiving a packet the receiver can be in one of these
	 *    states.
	 */

#define RS_SYNC 0	    /*	Waiting for sync		*/
#define RS_CTL	1	    /*	Waiting for command		*/
#define RS_CCHK 2	    /*	Waiting for check byte		*/
#define RS_LEN1 3	    /*	Waiting for MSB length byte	*/
#define RS_LEN2 4	    /*	Waiting for LSB length byte	*/
#define RS_DATA 5	    /*	Waiting for data & checksum	*/

	/*
	 *    The low level protocol generates packets.   This is used 
	 *    for error checking, data sequencing, retries, restart, etc...
	 *    The packet format is:
	 *
	 *	SYNC sss0xccc CHK
	 *	SYNC sss1xccc CHK nnnnnnnn nnnnnnnn [DATA] CHK2
	 *			    msb       lsb
	 *
	 *	sss = sequence #
	 *	B4  = packet contains data
	 *	B3  = reserved (may be used to extend the command set)
	 *	ccc = PKF_?????
	 *
	 *	NOTE that the data length nnn..nn is not checked with either
	 *	CHK or CHK2 .  The protocol will determine if the length
	 *	is reasonable (< MAXPKT) and then try to read that many
	 *	bytes.  The CHK will obviously fail if the length was 
	 *	incorrect.
	 */

#define PKF_SEQUENCE	0x07	/*  Sequence #			*/
#define PKF_MASK	0x78	/*  command mask		*/

#define PKCMD_RESTART 	0x20	/*  RESTART dnet.  (new)	*/
#define PKCMD_ACKRSTART	0x28	/*  ACK a restart  (new)	*/
#define PKCMD_WRITE6	0x30
#define PKCMD_WRITE	0x38	/*  A DATA packet		*/
#define PKCMD_CHECK	0x40	/*  Request ACK or NAK for win	*/
#define PKCMD_ACK	0x48	/*  ACK a window		*/
#define PKCMD_NAK	0x50	/*  NAK a window		*/
#define PKCMD_WRITE7	0x58

	/*
	 *  All channel multiplexing, channel commands, etc... is encoded
	 *  within a PKCMD_WRITE packet.
	 *
	 *  Channel commands are characterized by a one byte control
	 *  field and up to 7 bytes of data.  The control field format
	 *  is:		10cccnnn [DATA]		ccc = SCMD_??
	 *					nnn = # additional data bytes
	 */

#define SCMD_SWITCH	0x00	/*  switch active channel #	*/
#define SCMD_OPEN	0x01	/*  open a channel		*/
#define SCMD_CLOSE	0x02	/*  close a channel		*/
#define SCMD_ACKCMD	0x03	/*  ack an open/close request	*/
#define SCMD_EOFCMD	0x04	/*  Reof or Weof		*/
#define SCMD_QUIT	0x05	/*  QUIT dnet			*/
#define SCMD_IOCTL	0x06	/*  channel ioctl (new),PTY sup	*/
#define SCMD_RESERVE1	0x07

	/*
	 *  Stream data is characterized by the following format:
	 *
	 *		   msb      lsb	
	 *		11nnnnnn nnnnnnnn [128-16383 bytes DATA]
	 *			 0nnnnnnn [0-127 bytes DATA]
	 *
	 *  NOTE:  11000000 0ccccccc nnnnnnnn   reserved for furture ext. of
	 *					SCMD commands.
	 */

#define SCMD_DATA	0x08	/*  stream command, DATA (dummy ID)	*/

	/*
	 *  Each channel can be in one of several states.
	 */

#define CHAN_FREE	0x01	/*  free channel		*/
#define CHAN_ROPEN	0x02	/*  remote open, wait port msg	*/
#define CHAN_LOPEN	0x03	/*  local open, wait reply	*/
#define CHAN_OPEN	0x04	/*  channel open		*/
#define CHAN_CLOSE	0x05	/*  one side of channel closed  */

#define CHANF_ROK	0x01	/*  remote hasn't EOF'd us yet  */
#define CHANF_WOK	0x02	/*  remote will accept data	*/
#define CHANF_LCLOSE	0x04 	/*  channel closed on our side  */
#define CHANF_RCLOSE	0x08    /*  channel closed on rem side  */


#ifndef NOEXT
extern int DNet_fd;
extern PKT Pkts[9];
extern PKT *RPak[4];
extern PKT *WPak[4];
extern CHAN Chan[MAXCHAN];
extern LIST TxList;	       /*  For pending DNCMD_WRITE reqs.   */
extern fd_set Fdread;
extern fd_set Fdwrite;
extern fd_set Fdexcept;
extern void (*Fdstate[FD_SETSIZE])();
extern ubyte Fdperm[FD_SETSIZE];
extern uword FdChan[FD_SETSIZE];
extern ubyte RcvBuf[RCVBUF];
extern ubyte RTimedout, WTimedout;
extern uword Rto_act, Wto_act;
extern uword RcvData;
extern uword RExpect;
extern uword RChan;
extern uword WChan;
extern uword RPStart;
extern uword WPStart;
extern uword WPUsed;
extern uword RState;
extern ubyte DDebug;
extern ubyte PDebug;
extern ubyte Restart;
extern ubyte DeldQuit;
extern ulong NumCon; 
extern ubyte Mode7;

extern int errno;
extern int WReady;

#endif

#endif