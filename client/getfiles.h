#ifndef GETFILES_H
#define GETFILES_H
/*
 *
 *  DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved.
 *
 *  GETFILES [-dlocaldir] [-c] file/dir file/dir file/dir
 *
 *  -dlocaldir	local directory to place files
 *
 *  -c		Continue from where you left off before.  Files that already
 *		exist on the local machine will not be re-transfered.  This
 *		will also re-start in the middle of a file that was
 *		partially transfered previously.
 *
 *		This command assumes the file(s) on both machines have not
 *		been modified since the first attempt.	No other checking
 *		is done at the moment.
 */

typedef unsigned char ubyte;

typedef struct {
    char    Cmd;
    char    Str[64];
    long    Val;
} HDR;

#define BSSTR "\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010"


void fail(int code);

int CheckNoPath(register char *str);

int LostChannel(void);

int UnknownCmd(HDR *hdr);

/*
 *  retrieve a file.  If ContMode set and file exists, try to append to
 *  it.
 */

int GetFile(HDR *hdr, int stab);
/*
 *  Retrieve a directory.  Create it if necessary.
 */

int GetDir(HDR *hdr, int stab);

int WriteHeader(char c, char *str, long len);

int ReadHeader(HDR *hdr);

char * NSpaces(short n);

#endif