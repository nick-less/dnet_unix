
/*
 *  DNETLIB.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *  Library Interface for DNET.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <errno.h>
#ifdef O_CREAT
#include <sys/file.h>
#endif
#include "dnetlib.h"
#include <sys/un.h>

extern char *getenv();



char *getdirpart();

CHANN * DListen(uword port) {
    CHANN *chan;
    int s;
    SOCKADDR *sa = (SOCKADDR *)malloc(sizeof(SOCKADDR)+256);
    char *dirstr = getenv("DNETDIR") ? getenv("DNETDIR") : "";

    sprintf(sa->sa_data, NAMEPAT, dirstr, port);
    sa->sa_family = AF_UNIX;
    unlink(sa->sa_data);

    s = socket(PF_UNIX, SOCK_STREAM, 0);
    fcntl(s, F_SETOWN, getpid());
    if (bind(s, sa, sizeof(*sa)-sizeof(sa->sa_data)+strlen(sa->sa_data)) < 0) {
	close(s);
	free(sa);
	return(NULL);
    }
    if (listen(s, 5) < 0) {
	close(s);
	unlink(sa->sa_data);
	free(sa);
	return(NULL);
    }
    chan = (CHANN *)malloc(sizeof(CHANN));
    chan->s = s;
    chan->port = port;
    free(sa);
    return(chan);
}


void DUnListen(CHANN *chan)  {
    char *dirstr = getenv("DNETDIR") ? getenv("DNETDIR") : "";
    char buf[32];

    close(chan->s);
    sprintf(buf, NAMEPAT, dirstr, chan->port);
    unlink(buf);
    free(chan);
}

int DAccept(CHANN *chan) {
    SOCKADDR sa;
    int addrlen = sizeof(sa);
    int fd;

    fd = accept(chan->s, &sa, &addrlen);
    return(fd);
}

int DOpen(char *host, uword port, char txpri,  char rxpri) {
    int s;
    char rc;
    short xb[3];
    SOCKADDR *sa = (SOCKADDR *)malloc(sizeof(SOCKADDR)+256);
    char *dirstr = getenv("DNETDIR") ? getenv("DNETDIR") : "";

    if (rxpri < -127)
	rxpri = -127;
    if (rxpri > 126)
	rxpri = 126;
    if (txpri < -127)
	txpri = -127;
    if (txpri > 126)
	txpri = 126;

    if (host == NULL) {
		host = (getenv("DNETHOST")) ? getenv("DNETHOST"):"3";
	}

    sa->sa_family = AF_UNIX;
    sprintf(sa->sa_data, "%s%s%s", "/tmp/", "DNET.", host);

	socklen_t saddrlen = sizeof(struct sockaddr) + strlen(sa->sa_data);
    s = socket(PF_UNIX, SOCK_STREAM, 0);
    fcntl(s, F_SETOWN, getpid());
    if (connect(s, sa, saddrlen) < 0) {
		close(s);
		free(sa);
		return(-1);
    }
    free(sa);
    xb[0] = port;
    ((char *)&xb[1])[0] = txpri;
    ((char *)&xb[1])[1] = rxpri;
    write(s, xb, 4);
    if (read(s, &rc, 1) == 1 && rc == 0) {
		return(s);
	}
    close(s);
    return(-1);
}

void DEof(int fd) {
    char dummy;

    shutdown(fd, 1);
    write(fd, &dummy, 0);
}

int gwrite(int fd, char *buf, int bytes) {
    int n;
    int orig = bytes;
    extern int errno;
    while (bytes) {
	n = write(fd, buf, bytes);
	if (n > 0) {
	    bytes -= n;
	    buf += n;
	    continue;
	}
	if (n < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EWOULDBLOCK) {
		int wm = 1 << fd;
		int em = 1 << fd;
		if (select(fd+1, NULL, &wm, &em, NULL) < 0)
		    continue;
		if (wm)
		    continue;
	    }
	    return(orig - bytes);
	}
    }
    return(orig);
}

int gread(int fd, char *buf, int bytes) {
    int n;
    int orig = bytes;
    extern int errno;
    while (bytes) {
	n = read(fd, buf, bytes);
	if (n > 0) {
	    bytes -= n;
	    buf += n;
	    break;
	}
	if (n < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EWOULDBLOCK) {
		int rm = 1 << fd;
		int em = 1 << fd;
		if (select(fd+1, &rm, NULL, &em, NULL) < 0)
		    continue;
		if (rm)
		    continue;
	    }
	    return(orig - bytes);
	}
	if (n == 0)
	    break;
    }
    return(orig - bytes);
}

int ggread(int fd, char *buf, int bytes) {
    int n;
    int ttl = 0;
    while (bytes) {
	n = gread(fd, buf, bytes);
	if (n > 0) {
	    bytes -= n;
	    buf += n;
	    ttl += n;
	    continue;
	}
	return(-1);
    }
    return(ttl);
}

/*
 *	Convert to and from 68000 longword format.  Of course, it really
 *	doesn't matter what format you use, just as long as it is defined.
 */

ulong ntohl68(ulong n) {
    return(
	(((ubyte *)&n)[0] << 24)|
	(((ubyte *)&n)[1] << 16)|
	(((ubyte *)&n)[2] << 8)|
	(((ubyte *)&n)[3])
    );
}

ulong htonl68(ulong n) {
    ulong v;
    ((ubyte *)&v)[0] = n >> 24;
    ((ubyte *)&v)[1] = n >> 16;
    ((ubyte *)&v)[2] = n >> 8;
    ((ubyte *)&v)[3] = n;
    return(v);
}


int DoOption(short ac, char *av[], char *ops, char **args) {
    register short i;
    short j;

    for (i = j = 1; i < ac; ++i) {
	register char *ptr = av[i];
	if (*ptr != '-') {
	    av[j++] = av[i];
	    continue;
	}
	while (*++ptr) {
	    register char *op;
	    long **ap = (long **)&args;
	    short isshort;

	    for (op = ops; *op && *op != *ptr;) {
		if (*op == *ptr)
		    break;
		if (*++op == '%') {
		    while (*op && *op != 's' && *op != 'd')
			++op;
		    if (*op)
			++op;
		}
		if (*op == ',')     /*  optional ,  */
		    ++op;
		++ap;
	    }
	    if (*op == 0)
		return(-1);
	    if (op[1] != '%') {
		*(short *)*ap = 1;
		++ap;
		continue;
	    }
	    op += 2;
	    isshort = 1;
	    while (*op && *op != 's' && *op != 'd') {
		switch(*op) {
		case 'h':
		    isshort = 1;
		    break;
		case 'l':
		    isshort = 0;
		    break;
		default:
		    return(-1);
		}
		++op;
	    }
	    switch(*op) {
	    case 's':
		if (ptr[1]) {
		    *(char **)*ap = ptr + 1;
		    ptr = "\0";
		} else {
		    *(char **)*ap = av[++i];
		}
		break;
	    case 'd':
		if (isshort)
		    *(short *)*ap = atoi(++ptr);
		else
		    *(long *)*ap = atoi(++ptr);
		while (*ptr >= '0' && *ptr <= '9')
		    ++ptr;
		break;
	    default:
		return(-1);
	    }
	}
    }
    return(j);
}

void elog(int how, char *ctl, long arg) {
    char *dir = getenv("DNETDIR");
    FILE *fi;
    char buf[256];
    long dtime;

    time(&dtime);

    if (!dir)
	dir = "";
    sprintf(buf, "%s%s", dir, "DNET.LOG");
    if ((fi = fopen(buf, "a"))) {
		strcpy(buf, ctime(&dtime));
		buf[strlen(buf)-1] = 0;
		fprintf(fi, "%s ", buf);
		fprintf(fi, ctl, arg);
		putc('\n', fi);
		fclose(fi);
    }
    if (how == EFATAL) {
		exit(1);
	}
}

