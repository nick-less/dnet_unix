
/*
 *	SCOPY.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	Remote file copy server (putfiles is the client program)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include "servers.h"
#include "../lib/dnetlib.h"

char Buf[4096];

chandler()
{
    union wait stat;
    struct rusage rus;
    while (wait3(&stat, WNOHANG, &rus) > 0);
}

main(ac,av)
char *av[];
{
    long chann = DListen(PORT_FILECOPY);
    int fd;
    int n;
    char buf[256];
    extern int errno;

    elog(EDEBUG, "SCOPY START", 0);
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
        elog(EDEBUG, "SCOPY CONNECT", 0);
	if (fork() == NULL) {
	    putdir(fd, "."); 
	    _exit(1);
	}
	close(fd);
    }
    perror("SCOPY");
}

putdir(chan, dirname)
char *dirname;
{
    struct stat stat;
    char olddir[256];
    char co, nl, name[128];
    long len;
    int ret = -1;

    getwd(olddir);
    if (lstat(dirname, &stat) >= 0 && !(stat.st_mode & S_IFDIR)) {
	char rc = 'N';
	gwrite(chan, &rc, 1);
	elog(EWARN, "SCOPY: Unable to cd to dir '%s'", dirname);
	return(1);
    }
    if (chdir(dirname) < 0) {
	if (mkdir(dirname, 0777) < 0 || chdir(dirname) < 0) {
	    char rc = 'N';
	    elog(EWARN, "SCOPY: Unable to create directory '%s'", dirname);
	    gwrite(chan, &rc, 1);
	    return(1);
	}
    }
    co = 'Y';
    gwrite(chan, &co, 1);
    while (ggread(chan, &co, 1) == 1) {
	if (ggread(chan, &nl, 1) != 1 || ggread(chan, name, nl) != nl)
	    break;
	if (ggread(chan, &len, 4) != 4)
	    break;
	len = ntohl68(len);
	switch(co) {
	case 'C':
	    co = 'Y';
    	    if (chdir(name) < 0) {
		if (mkdir(name, 0777) < 0 || chdir(name) < 0)  {
		    co = 'N';
	            elog(EWARN, "SCOPY: Unable to create directory '%s'", 
			dirname);
		}
	    }
	    gwrite(chan, &co, 1);
	    break;
	case 'W':
	    if (putfile(chan, name, len) < 0) {
		ret = -1;
		char buffer [50+strlen(name)];
		sprintf(&buffer, "SCOPY: Failure on file %.*s %s", name, "%d");
		elog(EWARN, &buffer, len);
		goto fail;
	    }
	    break;
	case 'X':
	    if (putdir(chan, name) < 0) {
		ret = -1;
		goto fail;
	    }
	    break;
	case 'Y':
	    ret = 1;
	    co = 'Y';
	    gwrite(chan, &co, 1);
	    goto fail;
	default:
	    co = 'N';
	    gwrite(chan, &co, 1);
	    break;
	}
    }
fail:
    chdir(olddir);
    return(ret);
}

putfile(chan, name, len)
char *name;
{
    long fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    long n, r;
    char rc;

    if (fd < 0) {
	rc = 'N';
	gwrite(chan, &rc, 1);
	return(0);
    }
    rc = 'Y';
    gwrite(chan, &rc, 1);
    while (len) {
	r = (len > sizeof(Buf)) ? sizeof(Buf) : len;
	n = ggread(chan, Buf, r);
	if (n != r)
	    break;
        if (write(fd, Buf, n) != n)
	    break;
	len -= n;
    }
    close(fd);
    if (len) {
	unlink(name);
	return(-1);
    }
    rc = 'Y';
    gwrite(chan, &rc, 1);
    return(0);
}

