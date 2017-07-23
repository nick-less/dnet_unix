
/*
 *  GLOBALS.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 */

#include "dnet.h"

int Enable_Abort;

int DNet_fd = -1;	/*  Master listen socket	*/
PKT Pkts[9];		/*  data buffers for packets	*/
PKT *Raux = Pkts+8;	/*  next packet in		*/
PKT *RPak[4] = { Pkts+0,Pkts+1,Pkts+2,Pkts+3 };
PKT *WPak[4] = { Pkts+4,Pkts+5,Pkts+6,Pkts+7 };
CHAN Chan[MAXCHAN];    /*  Channels			   */
LIST TxList;	       /*  For pending SCMD_DATA reqs.     */
fd_set Fdread;
fd_set Fdwrite;
fd_set Fdexcept;
void (*Fdstate[FD_SETSIZE])();
ubyte Fdperm[FD_SETSIZE];
uword FdChan[FD_SETSIZE];
ubyte RcvBuf[RCVBUF];
uword RcvData;
uword RExpect;
ubyte RTimedout;
ubyte WTimedout;
uword WChan;		/*  Read and Write channels	    */
uword RChan;
uword RPStart;
uword WPStart;
uword WPUsed;
uword RState;
uword Rto_act, Wto_act;
ubyte DDebug;
ubyte PDebug;
ubyte Restart;
ubyte DeldQuit;
ulong NumCon;
ubyte Mode7 = 1;
int WReady;

