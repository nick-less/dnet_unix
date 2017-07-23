
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl_compat.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <strings.h>
#include "dnet.h"
#include "../server/servers.h"
#include "internal.h"


int isinternalport(uword port) {
    if (port == PORT_IALPHATERM)
	return(1);
    return(0);
}

int iconnect(int *ps, uword port) {
    if (port == PORT_IALPHATERM)
	return(ialphaterm_connect(ps, port));
    return(-1);
}

int ialphaterm_connect(int *pmaster, uword port) {
    struct sgttyb sg;
    struct tchars tc;
    struct ltchars ltc;
#ifdef TIOCGWINSZ
    struct winsize ws;
#else
#ifdef TIOCGSIZE
    struct ttysize ts;
#endif
#endif
    int lmode;
    int fdmaster;
    int fdslave;
    pid_t pid;
    char *slavename;

    ioctl(0, TIOCGETP, (char *)&sg);
    ioctl(0, TIOCGETC, (char *)&tc);
    ioctl(0, TIOCGLTC, (char *)&ltc);
    ioctl(0, TIOCLGET, (char *)&lmode);
#ifdef TIOCGWINSZ
    ioctl(0, TIOCGWINSZ, &ws);
#else
#ifdef TIOCGSIZE
    ioctl(0, TIOCGSIZE, &ts);
#endif
#endif

    sg.sg_flags &= ~(RAW);
    sg.sg_flags |= ECHO;
#ifdef TIOCGWINSZ
    ws.ws_row = 23;
    ws.ws_col = 77;
#else
#ifdef TIOCGSIZE
    ts.ts_lines = 23;
    ts.ts_cols = 77;
#endif
#endif

    if (DDebug)
	fprintf(stderr, "PTY openning internal pty\n");
    if (openpty(&fdmaster, &fdslave, &slavename) >= 0) {
	if (DDebug)
	    fprintf(stderr, "PTY open successfull\n");
	if ((pid = fork()) == 0) {
	    int i;
	    setenv("DNET", "IALPHATERM", 1);
	    setuid(getuid());
	    signal(SIGHUP, SIG_DFL);
	    signal(SIGINT, SIG_DFL);
	    signal(SIGQUIT, SIG_DFL);
	    signal(SIGTERM, SIG_DFL);
	    signal(SIGCHLD, SIG_DFL);
	    signal(SIGTSTP, SIG_IGN);
	    ioctl(open("/dev/tty", 2), TIOCNOTTY, NULL);
	    close(open(slavename, 0));
	    dup2(fdslave, 0);
	    dup2(0, 1);
	    dup2(0, 2);
	    for (i = 3; i < 256; ++i)
		close(i);
	    ioctl(0, TIOCSETN, &sg);
	    ioctl(0, TIOCSETC, &tc);
	    ioctl(0, TIOCSLTC, &ltc);
	    ioctl(0, TIOCLSET, &lmode);
#ifdef TIOCSWINSZ
	    ioctl(0, TIOCSWINSZ, &ws);
#else
#ifdef TIOCSSIZE
	    ioctl(0, TIOCSSIZE, &ts);
#endif
#endif
	    {
		char *shell = getenv("SHELL");
		char *home = getenv("HOME");
		if (!shell)
		    shell = "/bin/sh";
		if (!home)
		    home = ".";
		chdir(home);
		execl(shell, "-fshell", NULL);
		perror(shell);
	    }
	    _exit(1);
	}
	if (pid > 0) {
	    *pmaster = fdmaster;
	    close(fdslave);
	    if (DDebug)
		fprintf(stderr, "PTY OPEN OK.2\n");
	    return(1);
	}
	close(fdmaster);
	close(fdslave);
	if (DDebug)
	    fprintf(stderr, "PTY OPEN FAILURE.1\n");
    }
    if (DDebug)
	fprintf(stderr, "PTY OPEN FAILURE.2\n");
    return(-1);
}

int openpty(int *pfdm, int *pfds, char **pnames) {
    static char ptcs[] = { "0123456789abcdef" };
    static char plate[] = { "/dev/ptyxx" };
    struct stat stat;
    int i;
    int j;

    for (i = 'p';; ++i) {
	plate[8] = i;
	plate[9] = ptcs[0];
	if (lstat(plate, &stat) < 0)
		break;
	for (j = 0; ptcs[j]; ++j) {
	    plate[9] = ptcs[j];
	    plate[5] = 'p';
	    if ((*pfdm = open(plate, O_RDWR)) >= 0) {
		plate[5] = 't';
		if ((*pfds = open(plate, O_RDWR)) >= 0) {
		    *pnames = plate;
		    if (DDebug)
			fprintf(stderr, "PTY FOUND %s\n", *pnames);
		    return(1);
		}
		close(*pfdm);
	    }
	}
    }
    return(-1);
}

int isetrows(int fd, int rows) {
#ifdef TIOCSWINSZ
    struct winsize ws;
    if (ioctl(fd, TIOCGWINSZ, &ws) >= 0) {
	ws.ws_row = rows;
	ioctl(fd, TIOCSWINSZ, &ws);
    }
#else
#ifdef TIOCSSIZE
    struct ttysize ts;
    if (ioctl(fd, TIOCGSIZE, &ts) >= 0) {
	ts.ts_lines = rows;
	ioctl(fd, TIOCSSIZE, &ts);
    }
#endif
#endif
return 0;
}

int isetcols(int fd, int cols) {
#ifdef TIOCSWINSZ
    struct winsize ws;
    if (ioctl(fd, TIOCGWINSZ, &ws) >= 0) {
	ws.ws_col = cols;
	ioctl(fd, TIOCSWINSZ, &ws);
    }
#else
#ifdef TIOCSSIZE
    struct ttysize ts;
    if (ioctl(fd, TIOCGSIZE, &ts) >= 0) {
	ts.ts_cols = cols;
	ioctl(fd, TIOCSSIZE, &ts);
    }
#endif
#endif
return 0;
}

