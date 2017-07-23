#ifndef DNETLIB_H
#define DNETLIB_H

typedef unsigned short uword;
typedef unsigned long ulong;
typedef unsigned char ubyte;
typedef struct sockaddr SOCKADDR;

typedef struct {
	int s;
	uword port;
} CHANN;

#define NAMELEN sizeof("/tmp/.PORT.XXXXX")
#define NAMEPAT "/tmp/%s.PORT.%ld"

#define EFATAL 0
#define EWARN 1
#define EDEBUG 2


CHANN * DListen(uword port);

void DUnListen(CHANN *chan);

int DAccept(CHANN *chan);

int DOpen(char *host, uword port, char txpri,  char rxpri);

void DEof(int fd);

int gwrite(int fd, char *buf, int bytes);

int gread(int fd, char *buf, int bytes);

int ggread(int fd, char *buf, int bytes);

/*
 *	Convert to and from 68000 longword format.  Of course, it really
 *	doesn't matter what format you use, just as long as it is defined.
 */

ulong ntohl68(ulong n);

ulong htonl68(ulong n);

int DoOption(short ac, char *av[], char *ops, char **args);

void elog(int how, char *ctl, long arg);
#endif