
/*
 *  PACKET.C
 */

#include <strings.h>
#include "dnet.h"
#include "dataconv.h"
#include "subs.h"
#include "control.h"

#include "net.h"

#include "packet.h"


ubyte	RxTmp[MAXPACKET];

void BuildDataPacket(PKT *pkt, ubyte win, ubyte *dbuf, uword actlen) {
    ubyte *ptr;
    ubyte *pend;
    ubyte range = 0;
    static ubyte BadCtl[32] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1
    };

    ptr = dbuf;
    pend= dbuf + actlen;

    while (ptr < pend) {
	if (*ptr < 0x20 && BadCtl[*ptr])
	    range |= 1;
	if (*ptr >= 0x80)
	    range |= 2;
	++ptr;
    }
    if (Mode7 && range) {
	uword buflen = Expand6(dbuf, pkt->data, actlen);
	BuildPacket(pkt, PKCMD_WRITE6, win, pkt->data, actlen, buflen);
    } else {
	if (Mode7 || (range & 2)) {
	    BuildPacket(pkt, PKCMD_WRITE, win, dbuf, actlen, actlen);
	} else {
	    uword buflen = Compress7(dbuf, pkt->data, actlen);
	    BuildPacket(pkt, PKCMD_WRITE7, win, pkt->data, actlen, buflen);
	}
    }
}

PKT * BuildRestartAckPacket(ubyte *dbuf, ubyte bytes) {
    static PKT pkt;
    BuildPacket(&pkt, PKCMD_ACKRSTART, 0, dbuf, bytes, bytes);
    return(&pkt);
}

void BuildPacket(PKT *pkt, ubyte ctl, ubyte win, ubyte *dbuf, uword actlen, uword buflen) {
    pkt->buflen = buflen;
    pkt->sync = SYNC;
    pkt->ctl = ctl | win;
    pkt->cchk = Ascize((pkt->sync << 1) ^ pkt->ctl);
    if (actlen) {
		uword chk = chkbuf(dbuf, buflen);
		ubyte dchkh = chk >> 8;
		ubyte dchkl = chk;

		pkt->lenh = Ascize(actlen >> 6);
		pkt->lenl = Ascize(actlen);
		pkt->dchkh= Ascize(dchkh);
		pkt->dchkl= Ascize(dchkl);
		if (dbuf != pkt->data) {
			bcopy(dbuf, pkt->data, buflen);

		}
    }
}

void WritePacket(PKT *pkt) {

    if (DDebug) {
		fprintf(stderr, "SEND-PACKET %02x %d\n", pkt->ctl, pkt->buflen);
	}

    if (pkt->buflen) {
		NetWrite(&pkt->sync, 7 + pkt->buflen);
	} else {
		NetWrite(&pkt->sync, 3);
	}
    switch(pkt->ctl) {
    case PKCMD_WRITE:
    case PKCMD_WRITE6:
    case PKCMD_WRITE7:
    case PKCMD_RESTART:
	WTimeout(WTIME);
    }
}

void WriteCtlPacket(int ctl, ubyte win) {
    static CTLPKT pkt;

    NetWrite(NULL, 0);
    BuildPacket((PKT*)&pkt, ctl, win, NULL, 0, 0);
    WritePacket((PKT*)&pkt);
}

void WriteNak(ubyte win) {
    WriteCtlPacket(PKCMD_NAK, win);
}

void WriteAck(ubyte win) {
    WriteCtlPacket(PKCMD_ACK, win);
}

void WriteChk(ubyte win) {
    WriteCtlPacket(PKCMD_CHECK, win);
}

void WriteRestart() {
    WriteCtlPacket(PKCMD_RESTART, 0);
}

/*
 *	RECEIVE A PACKET
 */

int RecvPacket(ubyte *ptr, long len) {
    static uword ActLen;    /*	actual # bytes after decoding */
    static uword BufLen;    /*	length of input data buffer   */
    static uword DBLen;     /*	# bytes already in i.d.buf    */
    static ubyte RState;
    static PKT	 Pkt;
    ubyte  *dptr;
    short expect;

    if (ptr == NULL) {
	RState = 0;
	return(3);
    }

    while (len) {
	switch(RState) {
	case 0:
	    --len;
	    Pkt.sync = *ptr++;

	    if (Pkt.sync == SYNC)
		++RState;
	    break;
	case 1: 	/*  CTL */
	    --len;
	    Pkt.ctl = *ptr++;

	    if (Pkt.ctl < 0x20 || Pkt.ctl > 0x7F) {
		RState = 0;
		break;
	    }
	    ++RState;
	    break;
	case 2: 	/*  CCHK    */
	    --len;
	    Pkt.cchk = *ptr++;

	    if (Ascize((SYNC<<1) ^ Pkt.ctl) != Pkt.cchk) {
		RState = 0;
		break;
	    }
	    switch(Pkt.ctl & PKF_MASK) {
	    case PKCMD_ACKRSTART:
	    case PKCMD_WRITE:
	    case PKCMD_WRITE6:
	    case PKCMD_WRITE7:
		if (DDebug)
		    printf("Recv Header %02x\n", Pkt.ctl & PKF_MASK);
		++RState;
		break;
	    default:
		if (DDebug)
		    printf("Recv Control %02x\n", Pkt.ctl & PKF_MASK);
		do_cmd(Pkt.ctl, NULL, 0);
		RState = 0;
		break;
	    }
	    break;
	case 3: 	/*  LENH    */
	    --len;
	    Pkt.lenh = *ptr++;

	    ++RState;
	    if (Pkt.lenh < 0x20 || Pkt.lenh > 0x7F)
		RState = 0;
	    break;
	case 4: 	/*  LENL    */
	    --len;
	    Pkt.lenl = *ptr++;

	    if (Pkt.lenl < 0x20 || Pkt.lenl > 0x7F) {
		RState = 0;
		break;
	    }

	    ++RState;

	    ActLen = ((Pkt.lenh & 0x3F) << 6) | (Pkt.lenl & 0x3F);
	    DBLen = 0;

	    switch(Pkt.ctl & PKF_MASK) {
	    case PKCMD_ACKRSTART:
	    case PKCMD_WRITE:
		BufLen = ActLen;
		break;
	    case PKCMD_WRITE6:
		BufLen = (ActLen * 8 + 5) / 6;
		break;
	    case PKCMD_WRITE7:
		BufLen = (ActLen * 7 + 7) / 8;
		break;
	    default:
		puts("BaD");
		break;
	    }

	    if (ActLen > MAXPKT || BufLen > MAXPACKET) {
		if (DDebug || DDebug)
		    printf("Packet Length Error %d %d\n", ActLen, BufLen);
		RState = 0;
	    }
	    break;
	case 5: 	/*  DChkH   */
	    --len;
	    Pkt.dchkh = *ptr++;

	    ++RState;
	    if (Pkt.dchkh < 0x20 || Pkt.dchkh > 0x7F)
		RState = 0;
	    break;
	case 6: 	/*  DCHKL   */
	    --len;
	    Pkt.dchkl = *ptr++;

	    ++RState;
	    if (Pkt.dchkl < 0x20 || Pkt.dchkl > 0x7F)
		RState = 0;
	    break;
	case 7: 	/*  -DATA-  */
	    if (DBLen + len < BufLen) {     /*  not enough  */
		bcopy(ptr, Pkt.data + DBLen, len);
		DBLen += len;
		len = 0;
		break;
	    }

	    /*
	     *	Enough data, check chk
	     */

	    bcopy(ptr, Pkt.data + DBLen, BufLen - DBLen);
	    len -= BufLen - DBLen;
	    ptr += BufLen - DBLen;

	    {
		uword chk;
		ubyte chkh;
		ubyte chkl;

		chk = chkbuf(Pkt.data, BufLen);
		chkh = Ascize(chk >> 8);
		chkl = Ascize(chk);

		if (Pkt.dchkh != chkh || Pkt.dchkl != chkl) {
		    printf("Chksum failure %02x %02x %02x %02x\n",
			Pkt.dchkh, chkh, Pkt.dchkl, chkl
		    );
		    RState = 0;
		    break;
		}
	    }

	    switch(Pkt.ctl & PKF_MASK) {
	    case PKCMD_ACKRSTART:
		dptr = Pkt.data;
		break;
	    case PKCMD_WRITE:
		dptr = Pkt.data;
		break;
	    case PKCMD_WRITE6:
		UnExpand6(Pkt.data, RxTmp, ActLen);
		dptr = RxTmp;
		break;
	    case PKCMD_WRITE7:
		UnCompress7(Pkt.data, RxTmp, ActLen);
		dptr = RxTmp;
		break;
	    default:
		puts("BaD2");
		dptr = Pkt.data;
		break;
	    }
	    if (DDebug)
		printf("Recv Body   %02x %d bytes\n", Pkt.ctl, ActLen);

	    do_cmd(Pkt.ctl, dptr, ActLen);

	    RState = 0;
	    break;
	}
    }

    {
	static short ExpAry[] = { 3, 2, 1, 4, 3, 2, 1, 0 };

	expect = ExpAry[RState];

	if (RState == 7)
	    expect = BufLen - DBLen;
    }
#ifdef NOTDEF
    if (Rto_act) {
	AbortIO((IOR *)&Rto);
	WaitIO((IOR *)&Rto);
	Rto_act = 0;
    }
    if (RState == 7) {
	Rto.tr_time.tv_secs = 8;
	Rto.tr_time.tv_micro= 0;
	SendIO((IOR *)&Rto);
	Rto_act = 1;
    }
#endif
    do_cmd((uword)-1, NULL, 0);
    return((int)expect);
}

