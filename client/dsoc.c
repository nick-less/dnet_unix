
/*
 *	DSOC.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	DSOC [port#]
 *
 *      Connect to the specified port# .. Used to connect to a remote CLI
 *	(s_shell server on the Amiga, which requires PIPE: to work, port 8196,
 *	is the default)
 *
 *	Uses standard cooked mode instead of RAW mode.
 */


#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "../server/servers.h"
#include "../lib/dnetlib.h"

int fd;
char buf[2048];

void handler(int sig);

int main(int ac,char *av[] ) {
    int n;
    int port = (av[1]) ? atoi(av[1]) : PORT_AMIGASHELL;

    puts("DSOC V1.01 11 March 1988 Connecting");
    fd = DOpen(NULL, port, 0, 0);
    if (fd < 0) {
        perror("DOpen");
        exit(1);
    }
    puts("Connected");
    signal(SIGIO, handler);
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, FNDELAY|FASYNC);
    while ((n = gread(0, buf, sizeof(buf))) > 0) {
    	gwrite(fd, buf, n);
    }
    fprintf(stderr, "EOF\n");
    DEof(fd);
    for (;;)
	pause();
}

void handler(int sig) {
    int n;
    char buf[1024];

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
    	write(1, buf, n);
    }
    if (n == 0) {
        write(1, "REMEOF\n", 7);
        exit(1);
    }
}

