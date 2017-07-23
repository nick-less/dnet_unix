#ifndef CONTROL_H
#define CONTROL_H
/*
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

void do_rto(void);

/*
 *  WTO:    Write timeout (unresolved output windows exist).  Resend a CHECK
 *	    command for all unresolved (READY) output windows
 */

void do_wto(void);

/*
 *  RNET:   Receved data ready from network.  The RNet request will
 *	    automagically be reissued on return.
 */

void do_rnet(void);


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

int do_wupdate(void) ;

void dumpcheck(register ubyte *ptr) ;

void do_cmd(short ctl, ubyte *buf, int bytes) ;

void do_rupdate(void);
void do_reccmd(ubyte cmd, ubyte *ptr, int len);

void replywindow(ubyte window) ;

#endif