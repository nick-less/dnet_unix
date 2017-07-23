#ifndef SUBS_H
#define SUBS_H
/*
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	Support subroutines
 *
 */


/*
 *   WRITESTREAM()
 *
 *	Queues new SCMD_?? level commands to be sent
 */

void WriteStream(long sdcmd, ubyte *buf, long len, uword chan);

int alloc_channel(void);

/*
 *    Remove all nodes with the given channel ID.
 */

void ClearChan(LIST *list, uword chan, int all);

/*
 *  Queue a packet into a prioritized list.  FIFO is retained for packets
 *  of the same priority.  This implements one level of channel priorities,
 *  before the packets actually get queued to the network.  Since up to
 *  4 packets might be queued (200 * 4 = 800 bytes of data or 4 seconds @
 *  2400 baud), a second level of prioritization will also reduce the
 *  physical packet size when two channels at relatively large differing
 *  priorities are in use.
 *
 *	These and other list routines compatible with Amiga list routines.
 */

void Enqueue(LIST *list, XIOR *ior);

void AddTail(LIST *list, NODE *node);

void AddHead(LIST *list, NODE *node);

ubyte *RemHead(LIST *list);

void NewList(LIST *list);

NODE *GetNext(NODE *node);

/*
 *  CHKBUF
 *
 *	Checksum a buffer.  Uses a simple, but supposedly very good
 *	scheme.
 */

int chkbuf(register ubyte *buf, register uword bytes);

/*
 *   Write timeout signal handler.
 */

void sigwto(int sigs) ;

void TimerOpen(void);

void TimerClose(void);

void WTimeout(int secs);

void dneterror(char *str);

/*
 *    setenv(name, str).  name must be of the form "NAME="
 */

void setenvXXX(char *name, char *str);

void startserver(uword port);

int scan_for_server(FILE *fi, long port);

void checktilda(char *buf);


#endif
