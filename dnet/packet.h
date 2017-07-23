
#ifndef PACKET_H
#define PACKET_H

#define Ascize(foo) (((foo) & 0x3F) | 0x40)

void BuildPacket(PKT *pkt, ubyte ctl, ubyte win, ubyte *dbuf, uword actlen, uword buflen);
void BuildDataPacket(PKT *pkt, ubyte win, ubyte *dbuf, uword actlen);
PKT * BuildRestartAckPacket(ubyte *dbuf, ubyte bytes);

void WritePacket(PKT *pkt);

void WriteCtlPacket(int ctl, ubyte win);

void WriteNak(ubyte win);

void WriteAck(ubyte win);

void WriteChk(ubyte win);

void WriteRestart();
/*
 *	RECEIVE A PACKET
 */

int RecvPacket(ubyte *ptr, long len);

#endif