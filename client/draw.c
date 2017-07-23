
/*
 *	DRAW.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	DRAW [port#]
 *
 *	Put terminal into RAW mode and connect to the remote dnet port.
 *	used mainly to test DNET.  Can also be used open a TERM window on
 *	the amiga (via the STERM server), which is the default (port 8195)
 */



#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include "../server/servers.h"

int fd;
char buf[4096];

main(ac,av)
char *av[];
{
    int n;
    extern int handler();
    int port = (av[1]) ? atoi(av[1]) : PORT_AMIGATERM;

    puts("DRAW V1.01 11 March 1988 Connecting");
    fd = DOpen(NULL, port, 0, 0);
    if (fd < 0) {
	perror("DOpen");
	exit(1);
    }
    puts("Connected");
    signal(SIGIO, handler);
    ttyraw();
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, FNDELAY|FASYNC);
    fcntl(0,  F_SETFL, FNDELAY);
    while ((n = gread(0, buf, sizeof(buf))) > 0) {
	gwrite(fd, buf, n);
    }
    fprintf(stderr, "EOF\n");
    DEof(fd);
    for (;;)
	pause();
}

handler()
{
    int n;
    char buf[1024];

    while ((n = read(fd, buf, sizeof(buf))) > 0) 
	write(1, buf, n);
    if (n == 0) {
	write(1, "REMEOF\n", 7);
	ttynormal();
	exit(1);
    }
}

static struct sgttyb ttym;

ttyraw()
{
    ioctl (0, TIOCGETP, &ttym);
    ttym.sg_flags |= RAW;
    ttym.sg_flags &= ~CBREAK;
    ttym.sg_flags &= ~ECHO;
    ioctl (0, TIOCSETP, &ttym);
}

ttynormal()
{
    ioctl (0, TIOCGETP, &ttym);
    ttym.sg_flags &= ~RAW;
    ttym.sg_flags |= CBREAK;
    ttym.sg_flags |= ECHO;
    ioctl (0, TIOCSETP, &ttym);
}

