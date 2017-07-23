
/*
 *  SGCOPY.C	 V1.1
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "servers.h"
#include "../dnet/net.h"
#include "../lib/dnetlib.h"
#include "sgcopy.h"




char Buf[4096];
int Chan;

void chandler(int sig) {
    union wait stat;
    struct rusage rus;
    while (wait3(&stat, WNOHANG, &rus) > 0);
}

int main(int ac,char *av[]) {
    CHANN *chann = DListen(PORT_GFILECOPY);
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
	    SGCopy(fd);
	    _exit(1);
	}
	close(fd);
    }
    perror("SCOPY");
}

int SGCopy(int fd) {
    short error = 0;
    static HDR Hdr;

    Chan = fd;
    error = WriteHeader('H', "Hello, GCopy server V1.30", 0);
    if (error)
	return(error);
    switch(ReadHeader(&Hdr)) {
    default:
    case -1:
	error = 1;
	return(error);
    case 'H':
	break;
    }
    while (!error) {
	switch(ReadHeader(&Hdr)) {
	case 'G':
	    {
		char svdir[1024];
		getwd(svdir);
		if (chdir(getdirpart(Hdr.Str)) < 0) {
		    error = WriteHeader('N', "Unable to cd to dir", 0);
		} else {
	    	    error = PutObject(getnamepart(Hdr.Str));
		}
		chdir(svdir);
	    }
	    break;
	case 'E':
	    goto done;
	case 'P':   /*  put-files, not implemented  */
	default:
	    error = 1;
	    break;
	}
    }
done:
    ;
}

int PutObject(char *str) {
    struct stat stat;
    short error = 0;

    if (lstat(str, &stat) < 0) {
	error = WriteHeader('N', "Unable to find object", 0);
	return(0);
    }
    if (stat.st_mode & S_IFDIR) {
	error = PutDir(str);
    } else {
	error = PutFile(str);
    }
    return(0);
}

int PutDir(char *name) {
    struct stat stat;
    char svdir[1024];
    static HDR Hdr;
    short error = 0;
    char *fn = getnamepart(name);
    DIR *dir;
    struct direct *de;

    if (lstat(name, &stat) < 0 || !(dir = opendir(name))) {
	WriteHeader('N', "Possible Disk Error", 0);
	error = 1;
	goto done;
    }
    if (error = WriteHeader('D', fn, 0)) 
	goto done;
    switch(ReadHeader(&Hdr)) {
    case 'Y':
	break;
    case 'S':
	goto done;
    case 'N':
	error = 1;
	break;
    default:
	error = 1;
	break;
    }
    if (error)
	goto done;

    getwd(svdir);
    if (chdir(name) < 0) {
	error = 1;
	WriteHeader('N', "unable to chdir", 0);
    }
    if (error)
	goto done;

    while (de = readdir(dir)) {
	if (strcmp(de->d_name, ".") == 0)
	    continue;
	if (strcmp(de->d_name, "..") == 0)
	    continue;
	if (lstat(de->d_name, &stat) < 0) {
	    continue;
	}
	if (stat.st_mode & S_IFDIR) {
	    error = PutDir(de->d_name);
	} else {
	    error = PutFile(de->d_name);
	}
	if (error)
	    break;
    }
    WriteHeader('E', NULL, 0);
    chdir(svdir);
done:
    return(error);
}

int PutFile(char *name) {
    int fd = -1;
    static HDR Hdr;
    long len;
    short error = 0;
    char *fn = getnamepart(name);

    fd = open(fn, O_RDONLY, 0);
    if (fd < 0) {       /*  don't do anything if unable to open it */
	WriteHeader('N', "file not readable", 0);
	goto done;
    }
    len = lseek(fd, 0L, 2);
    if (error = WriteHeader('F', fn, len))
	goto done;
    switch(ReadHeader(&Hdr)) {
    case 'Y':
	lseek(fd, Hdr.Val, 0);  /*  start pos.  */
	len -= Hdr.Val;
	if (len < 0)
	    len = 0;
	break;
    case 'S':
	goto done;
    case 'N':
	error = 1;
	break;
    default:
	error = 1;
	break;
    }
    if (error)
	goto done;
    while (len) {
	register long n = (len > sizeof(Buf)) ? sizeof(Buf) : len;

	if (read(fd, Buf, n) != n) {    /*  read failed! */
	    error = 10;
	    goto done;
	}
	if (gwrite(Chan, (unsigned char*)&Buf, n) != n) {
	    error = 10;
	    goto done;
	}
	len -= n;
    }
done:
    if (fd >= 0)
	close(fd);
    return(error);
}


int WriteHeader(char c, char *str, long len) {
    ubyte sl;

    if (str == NULL)
	str = "";
    sl = strlen(str);

    if (gwrite(Chan, (unsigned char*)&c, 1) < 0)
	return(1);
    if (gwrite(Chan, (unsigned char*)&sl,1) < 0)
	return(1);
    if (gwrite(Chan, (unsigned char*)str, sl) != sl)
	return(1);
    len = htonl68(len);
    if (gwrite(Chan, (unsigned char*)&len, 4) != 4) {
    	return(1);
    }
    return(0);
}

int ReadHeader(HDR *hdr) {
    ubyte sl;
    ubyte cmd;

    hdr->Cmd = -1;
    if (ggread(Chan, &cmd, 1) != 1)
	return(-1);
    if (ggread(Chan, &sl, 1) != 1)
	return(-1);
    if (sl >= sizeof(hdr->Str)) {
	return(-1);
    }
    if (ggread(Chan, hdr->Str, sl) != sl)
	return(-1);
    hdr->Str[sl] = 0;
    if (ggread(Chan, &hdr->Val, 4) != 4)
	return(-1);
    hdr->Val = ntohl68(hdr->Val);
    hdr->Cmd = cmd;
    return(hdr->Cmd);
}

char *getnamepart(char *str) {
    register char *ptr = str + strlen(str);

    while (ptr >= str) {
	if (*ptr == '/')
	    break;
	--ptr;
    }
    return(ptr+1);
}

char *getdirpart(char *str) {
    static char buf[1024];

    strcpy(buf, str);
    getnamepart(buf)[0] = 0;
    if (buf[0] == 0)
	return(".");
    return(buf);
}
