#ifndef SGCOPY_H
#define SGCOPY_H
/*
 *
 *  DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved.
 *
 *  GET-COPY SERVER	(NEW COPY SERVER)
 *
 *  The current version only accepts one connection at a time.	This server
 *  will send requested files to the remote machine.
 *
 *  length in 68000 longword format.
 */



typedef struct {
    char    Cmd;
    char    Str[64];
    long    Val;
} HDR;

typedef unsigned char ubyte;


void chandler(int sig) ;


int SGCopy(int fd) ;   

int PutObject(char *str);


int PutDir(char *name) ;
 
int PutFile(char *name) ;
 
int WriteHeader(char c, char *str, long len);
  
int ReadHeader(HDR *hdr) ;

char *getnamepart(char *str);
   
char *getdirpart(char *str);
   
#endif