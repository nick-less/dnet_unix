#ifndef INTERNAL_H
#define INTERNAL_H
/*
 *  INTERNAL.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	Usually SCMD_OPEN requests attempt to connect() to the UNIX
 *	domain socket of the server.  However, some 'ports' are designated
 *	as internal to DNET.  They reside here.
 *
 *	-IALPHATERM
 */




int isinternalport(uword port);

int iconnect(int *ps, uword port) ;

int ialphaterm_connect(int *pmaster, uword port);

int openpty(int *pfdm, int *pfds, char **pnames);

int isetrows(int fd, int rows);

int isetcols(int fd, int cols);

#endif
