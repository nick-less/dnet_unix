
/*
 *  NET.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *  NetWork raw device interface.  Replace with whatever interface you
 *  want.
 */

#include <unistd.h>
#include <sys/ioctl_compat.h>
#include "dnet.h"
#include <sys/stat.h>
#include "net.h"


void RcvInt (int sig, siginfo_t *info, void *ucontext) {
    int n = read(0, RcvBuf + RcvData, RCVBUF - RcvData);
    int i;
    extern int errno;

    errno = 0;
    if (n >= 0)
	RcvData += n;
    if (n <= 0)         /* disallow infinite fast-timeout select loops */
	RExpect = 0;
    if (DDebug)
	printf("read(%d,%d)\n", n, errno);
}

static struct sgttyb	ttym;
static struct stat	Stat;

void NetOpen(void) {
    int async = 1;

    fstat(0, &Stat);
    fchmod(0, 0600);
    /*
    signal(SIGIO, RcvInt);
    */
    ioctl (0, TIOCGETP, &ttym);
    if (Mode7) {
        ttym.sg_flags &= ~RAW;
        ttym.sg_flags |= CBREAK;
    } else {
        ttym.sg_flags |= RAW;
        ttym.sg_flags &= ~CBREAK;
    }
    ttym.sg_flags &= ~ECHO;
    ioctl (0, TIOCSETP, &ttym);
    /*
    ioctl (0, FIOASYNC, &async);
    */
    ioctl (0, FIONBIO, &async);
}

NetClose()
{
    int async = 0;

    fchmod(0, Stat.st_mode);
    ioctl (0, FIONBIO, &async);
    /*
    ioctl (0, FIOASYNC, &async);
    */
    ioctl (0, TIOCGETP, &ttym);
    ttym.sg_flags &= ~RAW;
    ttym.sg_flags |= ECHO;
    ioctl (0, TIOCSETP, &ttym);
}

void NetWrite(unsigned char *buf, long bytes) {
    if (DDebug)
	fprintf(stderr, "NETWRITE %08lx %ld\n", buf, bytes);
    gwrite(0, buf, bytes);
}

void gwrite(int fd, register unsigned char *buf, register long bytes) {
    register long n;
    while (bytes) {
	n = write(fd, buf, bytes);
	if (n > 0) {
	    bytes -= n;
	    buf += n;
	    continue;
	}
	if (errno == EINTR)
	    continue;
	if (errno == EWOULDBLOCK) {
	    fd_set fd_wr;
	    fd_set fd_ex;
	    FD_ZERO(&fd_wr);
	    FD_ZERO(&fd_ex);
	    FD_SET(fd, &fd_wr);
	    FD_SET(fd, &fd_ex);
	    if (select(fd+1, NULL, &fd_wr, &fd_ex, NULL) < 0) {
	 	perror("select");
	    }
	    continue;
	}
	if (errno == EPIPE)
	    return;
	dneterror("gwrite");
    }
}

