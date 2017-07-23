
/*
 *  DATACONV.C
 *
 *  Data Conversion.
 */

#include <stdio.h>

#include "dataconv.h"

extern ubyte DDebug;

int Compress7(ubyte *s, ubyte *db, uword n /*	actual source bytes */) {
    ubyte *d = db;

    while (n) {
	*d++ = *s++ << 1;

	if (--n == 0)
	    break;
	d[-1] |= *s >> 6;
	*d++ = *s++ << 2;

	if (--n == 0)
	    break;
	d[-1] |= *s >> 5;
	*d++ = *s++ << 3;

	if (--n == 0)
	    break;
	d[-1] |= *s >> 4;
	*d++ = *s++ << 4;

	if (--n == 0)
	    break;
	d[-1] |= *s >> 3;
	*d++ = *s++ << 5;

	if (--n == 0)
	    break;
	d[-1] |= *s >> 2;
	*d++ = *s++ << 6;

	if (--n == 0)
	    break;
	d[-1] |= *s >> 1;
	*d++ = *s++ << 7;

	if (--n == 0)
	    break;
	d[-1] |= *s++;
	--n;
    }
    return(d - db);
}
void UnCompress7(ubyte *s, ubyte *d, uword n /*	actual destination bytes */) {
    while (n) {
	*d++ = s[0] >> 1;

	if (--n == 0)
	    break;
	*d++ = ((s[0] << 6) | (s[1] >> 2)) & 0x7F;

	if (--n == 0)
	    break;
	*d++ = ((s[1] << 5) | (s[2] >> 3)) & 0x7F;

	if (--n == 0)
	    break;
	*d++ = ((s[2] << 4) | (s[3] >> 4)) & 0x7F;

	if (--n == 0)
	    break;
	*d++ = ((s[3] << 3) | (s[4] >> 5)) & 0x7F;

	if (--n == 0)
	    break;
	*d++ = ((s[4] << 2) | (s[5] >> 6)) & 0x7F;

	if (--n == 0)
	    break;
	*d++ = ((s[5] << 1) | (s[6] >> 7)) & 0x7F;

	if (--n == 0)
	    break;
	*d++ = s[6] & 0x7F;

	--n;
	s += 7;
    }
}

int Expand6(ubyte *s, ubyte *db, uword n) {
    ubyte *d = db;
    ubyte *sb= s;

    while (n) {
	*d++ = 0x40 | (*s >> 2);
	*d++ = 0x40 | ((*s++ & 3) << 4);
	if (--n) {
	    d[-1] |= *s >> 4;
	    *d++ = 0x40 | ((*s++ & 0x0F) << 2);
	    if (--n) {
		d[-1] |= *s >> 6;
		*d++ = 0x40 | (*s++ & 0x3F);
		--n;
	    }
	}
    }
    if (DDebug)
	printf("e6: %02x %02x -> %02x %02x %02x\n", 
	    sb[0], sb[1],
	    db[0], db[1], db[2]
	);
    return(d - db);
}

void UnExpand6(ubyte *s, ubyte *d, uword n) {

    ubyte *sb = s;
    ubyte *db = d;

    while (n) {
	*d++ = (s[0] << 2) | ((s[1] & 0x30) >> 4);
	if (--n == 0)
	    break;
	*d++ = (s[1] << 4) | ((s[2] & 0x3C) >> 2);
	if (--n == 0)
	    break;
	*d++ = (s[2] << 6) | (s[3] & 0x3F);
	--n;
	s += 4;
    }
    if (DDebug)
	printf("d6: %02x %02x <- %02x %02x %02x\n", 
	    db[0], db[1],
	    sb[0], sb[1], sb[2]
	);
}



