#ifndef DATACONV_H
#define DATACONV_H
/*
 *
 *  Data Conversion.
 */

typedef unsigned char ubyte;
typedef unsigned short uword;
typedef unsigned long ulong;


int Compress7(ubyte *s, ubyte *db, uword n); 

void UnCompress7(ubyte *s, ubyte *d, uword n);

int Expand6(ubyte *s, ubyte *db, uword n);

void UnExpand6(ubyte *s, ubyte *d, uword n);


#endif
