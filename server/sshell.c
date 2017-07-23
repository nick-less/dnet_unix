
/*
 *  S_SHELL.C		OBSOLETE OBSOLETE OBSOLETE
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *   Connect a csh to a pseudo terminal pair... PORT_ALPHATERM
 *   NOTE!!  PORT_IALPHATERM (a pseudo-terminal csh) is also available
 *   through the FTERM client program on the Amiga side, and much faster
 *   since the server for PORT_IALPHATERM is DNET itself (one less process
 *   to go through).
 *
 *	-doesn't handle SIGWINCH
 *	-doesn't handle flow control ... don't cat any long files!
 */

#include <stdio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <signal.h>

#include "servers.h"

chandler()
{
    union wait stat;
    struct rusage rus;
    while (wait3(&stat, WNOHANG, &rus) > 0);
}

main(ac,av)
char *av[];
{
    long chann = DListen(PORT_ALPHATERM);
    int fd;
    int n;
    char buf[256];
    extern int errno;

    if (av[1])
	chdir(av[1]);
    signal(SIGCHLD, chandler);
    signal(SIGPIPE, SIG_IGN);
    for (;;) {
	fd = DAccept(chann);
	if (fd < 0) {
	    if (errno == EINTR)
		continue;
	    break;
	}
	if (fork() == NULL) {
	    dup2(fd, 0);
	    dup2(fd, 1);
	    dup2(fd, 2);
	    close(fd);
	    execl("/usr/ucb/rlogin", "rlogin", "localhost", NULL);
	    exit(1);
	}
	close(fd);
    }
    perror("SSHELL");
}

