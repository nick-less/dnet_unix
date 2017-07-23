
/*
 *  SNFS.C	 V1.1
 *
 *  DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved.
 *
 *  NETWORK FILE SYSTEM SERVER
 *
 *  Accepts connections to files or directories & read-write or dir-scan calls.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "../dnet/net.h"
#include "../lib/dnetlib.h"
#include "servers.h"
#include "snfs.h"

/* #define DEBUG */

#define MAXHANDLES	256



int Chan;

typedef struct {
    short isopen;
    short fd;
    int   modes;
    int   remodes;
    long  pos;
    char  *name;
} HANDLE;

HANDLE Handle[MAXHANDLES];


void chandler(int sig)
{
    int stat;
    struct rusage rus;
    while (wait3(&stat, WNOHANG, &rus) > 0);
}


int main(int ac,char *av[]) {
    CHANN *chann = DListen(PORT_NFS);
    int fd;
    int n;
    char buf[1024];
    extern int errno;

    if (av[1]) {
		chdir(av[1]);
	}
#ifdef DEBUG
    freopen("NFS", "w", stderr);
    fprintf(stderr, "RUNNING\n");
    fflush(stderr);
#endif
    signal(SIGCHLD, chandler);
    signal(SIGPIPE, SIG_IGN);
    for (;;) {
	fd = DAccept(chann);
	if (fd < 0) {
	    if (errno == EINTR)
		continue;
	    break;
	}
	if (fork() == 0) {
	    NFs(fd);
#ifdef DEBUG
	    fprintf(stderr, "CLOSING\n");
	    fflush(stderr);
#endif
	    _exit(1);
	}
	close(fd);
    }
    perror("NFS");
}

void NFs(int chan) {
    OpenHandle("/", "", O_RDONLY); /* root */
    for (;;) {
	struct {
	    char	cmd;
	    unsigned char blen;
	    unsigned long dlen;
	} Base;
	long bytes;
	union {
	    OpOpen	Open;
	    OpRead	Read;
	    OpWrite	Write;
	    OpClose	Close;
	    OpSeek	Seek;
	    OpParent	Parent;
	    OpDelete	Delete;
	    OpCreateDir CreateDir;
	    OpDup	Dup;
	    OpNextDir   NextDir;
	    OpRename	Rename;
	} R;
	union {
	    RtOpen	Open;
	    RtRead	Read;
	    RtWrite	Write;
	    RtSeek	Seek;
	    RtParent	Parent;
	    RtDelete	Delete;
	    RtCreateDir CreateDir;
	    RtDup	Dup;
	    RtNextDir   NextDir;
	    RtRename	Rename;
	} W;
	long h;
	char buf[256];

	if (ggread(chan, (char*)&Base, sizeof(Base)) != sizeof(Base))
	    break;
#ifdef DEBUG
        fprintf(stderr, "command %02x %ld %ld\n", 
	    Base.cmd, Base.blen, Base.dlen
	);
	fflush(stderr);
#endif
	if (ggread(chan, (char*)&R, Base.blen) != Base.blen)
	    break;
	switch(Base.cmd) {
	case 'M':	/* create directory */
	    {
	        ggread(chan, buf, Base.dlen);
		AmigaToUnixPath(buf);
		mkdir(buf, 0777);
#ifdef DEBUG
		fprintf(stderr, "MakeDir %s\n", buf);
	        fflush(stderr);
#endif
	    }
	    R.Open.DirHandle = R.CreateDir.DirHandle;
	    /* FALL THROUGH */
	case 'P':
	    if (Base.cmd == 'P') {
		char *name = FDName(R.Parent.Handle);
		short i = strlen(name)-1;

#ifdef DEBUG
		fprintf(stderr, "Parent Dir of: %s\n", name);
		fflush(stderr);
#endif

		if (i >= 0 && name[i] == '/')	/* remove tailing /'s */
		    --i;
		if (i < 0) {
		    W.Open.Handle = -1;
	            gwrite(chan, (char*)&W.Open, sizeof(W.Open));
#ifdef DEBUG
		    fprintf(stderr, "NO PARENT\n");
		    fflush(stderr);
#endif
		    break;
		}
		while (i >= 0 && name[i] != '/')  /* remove name */
		    --i;
		while (i >= 0 && name[i] == '/')  /* remove tailing /'s */
		    --i;
		++i;
		if (i == 0) {	/* at root */
		    buf[i++] = '/';
		} 
		strncpy(buf, name, i);
		buf[i] = 0;
#ifdef DEBUG
		fprintf(stderr, "Parent Exists: %s\n", buf);
	        fflush(stderr);
#endif
	        R.Open.DirHandle = 0;
	    }
	    R.Open.Modes = 1005;
	    /* FALL THROUGH */
	case 'O':	/*	open	*/
	    if (Base.cmd == 'O')  {
	        ggread(chan, buf, Base.dlen);
		AmigaToUnixPath(buf);
#ifdef DEBUG
		fprintf(stderr, "OPEN: %s %d\n", buf, Base.dlen);
		fflush(stderr);
#endif
	    }
	    if (R.Open.Modes == 1006)
	        h = OpenHandle(FDName(R.Open.DirHandle),buf, 
		    O_CREAT|O_TRUNC|O_RDWR
		);
	    else
		h = OpenHandle(FDName(R.Open.DirHandle),buf, O_RDWR);
#ifdef DEBUG
	    fprintf(stderr, "Open h = %d name = %s  modes=%d\n", 
		h, buf, R.Open.Modes
	    );
	    fflush(stderr);
#endif
	    if (h >= 0) {
		struct stat stat;
		if (fstat(FDHandle(h), &stat) < 0)
		    perror("fstat");
	        W.Open.Handle = h;
	        W.Open.Prot = 0;
	        W.Open.Type = (stat.st_mode & S_IFDIR) ? 1 : -1;
#ifdef DEBUG
		fprintf(stderr, "fstat type %d\n", W.Open.Type);
		fflush(stderr);
#endif
	        W.Open.Size = stat.st_size;
		SetDate(&W.Open.Date, stat.st_mtime);
	        gwrite(chan, (char *)&W.Open, sizeof(W.Open));
		if (Base.cmd == 'P') {	/* tag name */
		    char *tail = TailPart(buf);
		    unsigned char c = strlen(tail) + 1;

		    gwrite(chan, (char*)&c, 1);
		    gwrite(chan, (char *)tail, c);
		}
	    } else {
		W.Open.Handle = -1;
	        gwrite(chan, (char *)&W.Open, sizeof(W.Open));
	    }
	    break;
	case 'N':	/* next directory.  Scan beg. at index	*/
	    {
		DIR *dir = opendir(FDName(R.NextDir.Handle));
		struct stat sbuf;
		struct direct *dp;
		long index = 0;
		char buf[1024];

		while (dir && index <= R.NextDir.Index + 2) {
		    if ((dp = readdir(dir)) == NULL)
			break;
		    ++index;
		}
		if (dir)
		    closedir(dir);
		if (index <= R.NextDir.Index + 2) {
		    W.Open.Handle = -1;
		} else {
		    W.Open.Handle = index;
		    strcpy(buf, FDName(R.NextDir.Handle));
		    strcat(buf, "/");
		    strcat(buf, dp->d_name);
		    stat(buf, &sbuf);
	            W.Open.Prot = 0;
	            W.Open.Type = (sbuf.st_mode & S_IFDIR) ? 1 : -1;
#ifdef DEBUG
		    fprintf(stderr, "fstat type %d\n", W.Open.Type);
		    fflush(stderr);
#endif
	            W.Open.Size = sbuf.st_size;
		    SetDate(&W.Open.Date, sbuf.st_mtime);
		}
		gwrite(chan, (char *)&W.Open, sizeof(W.Open));
		if (W.Open.Handle >= 0) {
		    unsigned char len = strlen(dp->d_name) + 1;
		    gwrite(chan, (char*)&len, 1);
		    gwrite(chan, (char *)dp->d_name, len);
		}
	    }
	    break;
	case 'r':	/*	RENAME	*/
	    {
		char tmp1[512];
		char tmp2[512];
		char buf1[1024];
		char buf2[1024];

	        ggread(chan, buf, Base.dlen);
		strcpy(tmp1, buf);
		strcpy(tmp2, buf + strlen(buf) + 1);
		AmigaToUnixPath(tmp1);
		AmigaToUnixPath(tmp2);
		ConcatPath(FDName(R.Rename.DirHandle1), tmp1, buf1);
		ConcatPath(FDName(R.Rename.DirHandle2), tmp2, buf2);
#ifdef DEBUG
		fprintf(stderr, "Rename %s to %s\n", buf1, buf2);
		fflush(stderr);
#endif
		if (rename(buf1, buf2) < 0)
		    W.Rename.Error = 1;
		else
		    W.Rename.Error = 0;
		gwrite(chan, (char *)&W.Rename.Error, sizeof(W.Rename.Error));
	    }
	    break;
	case 'd':	/*	DUP	*/
	    h = DupHandle(R.Dup.Handle);
	    if (h >= 0) {
		struct stat stat;
		if (fstat(FDHandle(h), &stat) < 0)
		    perror("fstat");
	        W.Open.Handle = h;
	        W.Open.Prot = 0;
	        W.Open.Type = (stat.st_mode & S_IFDIR) ? 1 : -1;
#ifdef DEBUG
		fprintf(stderr, "fstat type %d\n", W.Open.Type);
		fflush(stderr);
#endif
	        W.Open.Size = stat.st_size;
		SetDate(&W.Open.Date, stat.st_mtime);
	    } else {
		W.Open.Handle = -1;
	    }
	    gwrite(chan, (char *)&W.Dup, sizeof(W.Dup));
	    break;
	case 'R':	/*	READ	*/
	    {
		int fd = FDHandle(R.Read.Handle);
		char *buf = malloc(R.Read.Bytes);

		W.Read.Bytes = read(fd, buf, R.Read.Bytes);
#ifdef DEBUG
		fprintf(stderr, "h=%d fd %d Read %d  Result=%d\n", 
		    R.Read.Handle, fd, R.Read.Bytes, W.Read.Bytes
		);
	        fflush(stderr);
#endif
		gwrite(chan, (char *)&W.Read, sizeof(W.Read));
		if (W.Read.Bytes > 0)
		    gwrite(chan, (char *)buf, W.Read.Bytes);
		free(buf);
	    }
	    break;
	case 'W':
	    {
		int fd = FDHandle(R.Write.Handle);
		char *buf = malloc(R.Write.Bytes);
		if (ggread(chan, buf, R.Write.Bytes) != R.Write.Bytes)
		    break;
		W.Write.Bytes = write(fd, buf, R.Write.Bytes);
#ifdef DEBUG
		fprintf(stderr, "h=%d fd %d Write %d  Result=%d\n", 
		    R.Write.Handle, fd, R.Write.Bytes, W.Write.Bytes
		);
	        fflush(stderr);
#endif
		gwrite(chan, (char *)&W.Write, sizeof(W.Write));
		free(buf);
	    }
	    break;
	case 'C':
	    {
		CloseHandle(R.Close.Handle);
	    }
	    break;
	case 'S':
	    {
		int fd = FDHandle(R.Seek.Handle);
		W.Seek.OldOffset = lseek(fd, 0, 1);
		W.Seek.NewOffset = lseek(fd, R.Seek.Offset, R.Seek.How);
#ifdef DEBUG
		fprintf(stderr, "h %d SEEK %d %d %d result = %d\n",
		    R.Seek.Handle, fd, R.Seek.Offset, R.Seek.How,
		    W.Seek.NewOffset
		);
	        fflush(stderr);
#endif
		gwrite(chan, (char *)&W.Seek, sizeof(W.Seek));
	    }
	    break;
	case 'D':
	    {
		char buf2[1024];

	        ggread(chan, buf, Base.dlen);    /* get name to delete */
		AmigaToUnixPath(buf);
		ConcatPath(FDName(R.Delete.DirHandle), buf, buf2);

		unlink(buf2);
		rmdir(buf2);
#ifdef DEBUG
		fprintf(stderr, "Delete %s\n", buf2);
	        fflush(stderr);
#endif
		W.Delete.Error = 0;
		gwrite(chan, (char *)&W.Delete, sizeof(W.Delete));
	    }
	    break;
	default:
	    exit(1);
	    break;
	}
    }
}

int OpenHandle(char *base, char *tail, int modes) {
    short i;
    int fd;
    char name[1024];

    ConcatPath(base, tail, name);
    for (i = 0; i < MAXHANDLES; ++i) {
	if (Handle[i].isopen == 0)
	    break;
    }
    if (i == MAXHANDLES)
	return(-1);
    fd = open(name, modes, 0666);
    if (fd < 0 && (modes & O_RDWR) && !(modes & O_CREAT)) {
	modes &= ~O_RDWR;
	fd = open(name, modes);
    }
    Handle[i].name = strcpy(malloc(strlen(name)+1), name);
    Handle[i].fd = fd;
#ifdef DEBUG
    fprintf(stderr, "OpenHandle: %d = open %s %d\n", Handle[i].fd, name,modes);
    fflush(stderr);
#endif
    if (Handle[i].fd < 0)
	return(-1);
    Handle[i].modes = modes;
    Handle[i].remodes= modes & ~(O_TRUNC|O_CREAT);
    Handle[i].isopen = 1;
    return(i);
}

void CloseHandle(int h) {
#ifdef DEBUG
    fprintf(stderr, " Close Handle %d\n", h);
    fflush(stderr);
#endif
    if (h >= 0 && h < MAXHANDLES && Handle[h].isopen) {
	if (Handle[h].fd >= 0)
	    close(Handle[h].fd);
	Handle[h].fd = -1;
	Handle[h].isopen = 0;
	free(Handle[h].name);
    }
}

/*
 *  Insert ../ for / at beginning.
 */

void AmigaToUnixPath(char *buf) {
    char *base = buf;
#ifdef DEBUG
    fprintf(stderr, "AmigaToUnixPath %s", buf);
#endif
    if (*buf == ':')
	*buf++ = '/';
    while (*buf == '/') {
	bcopy(buf, buf + 2, strlen(buf)+1);
	buf[0] = buf[1] = '.';
	buf += 3;
    }
#ifdef DEBUG
    fprintf(stderr, " TO %s\n", base);
    fflush(stderr);
#endif
}

void ConcatPath(char *s1, char *s2, char *buf) {
#ifdef DEBUG
    fprintf(stderr, "ConCatPaths From '%s' '%s'\n", s1, s2);
#endif
    while (strncmp(s2, "../", 3) == 0) {	/* parent */
	;
	break;
    }
    while (strncmp(s2, "./", 2) == 0) {		/* current */
	s2 += 2;
    }
    if (s2[0] == '/') {
	strcpy(buf, s2);
	return;
    }
    if (s1[0] == 0 && s2[0] == 0) {
	strcpy(buf, ".");
	return;
    }
    if (s1[0] == 0)
	s1 = ".";
    strcpy(buf, s1);
    if (s1[strlen(s1)-1] != '/')
        strcat(buf, "/");
    strcat(buf, s2);
#ifdef DEBUG
    fprintf(stderr, "ConCatPaths to %s\n", buf);
    fflush(stderr);
#endif
}

char * FDName(int h) {
#ifdef DEBUG
    fprintf(stderr, "FDName(%d) =", h);
#endif
    if (h >= 0 && h < MAXHANDLES && Handle[h].isopen) {
#ifdef DEBUG
	fprintf(stderr, "%s\n", Handle[h].name);
        fflush(stderr);
#endif
	return(Handle[h].name);
    }
#ifdef DEBUG
    fprintf(stderr, "??\n");
    fflush(stderr);
#endif
    return(".");
}

int DupHandle(int h) {
    short n = -1;
    if (h >= 0 && h < MAXHANDLES && Handle[h].isopen)
	n = OpenHandle(".",Handle[h].name, Handle[h].remodes & ~O_RDWR);
    return(n);
}

int FDHandle(int h) {
    int fd = -1;
    if (h >= 0 && h < MAXHANDLES && Handle[h].isopen) {
	fd = Handle[h].fd;
	if (fd < 0) {
	    Handle[h].fd = fd = open(Handle[h].name, Handle[h].remodes, 0666);
	    if (fd >= 0 && !(Handle[h].modes & O_APPEND))
		lseek(fd, Handle[h].pos, 0);
	}
    }
    return(fd);
}

char *TailPart(char *path) {
    register char *ptr = path + strlen(path) - 1;

    while (ptr >= path && *ptr != '/')
	--ptr;
    ++ptr;
#ifdef DEBUG
    fprintf(stderr, "TAILPART '%s' -> %s\n", path, ptr);
    fflush(stderr);
#endif
    return(ptr);
}

void SetDate(STAMP *date, time_t mtime) {
    struct tm *tm = localtime(&mtime);
    long years = tm->tm_year;	/* since 1900	*/
    long days;

    years += 300;			/* since 1600   		*/
    days = (years / 400) * 146097;	/* # days every four cents	*/

    years = years % 400;

    /*
     *    First assume a leap year every 4 years, then correct for centuries.
     *    never include the 'current' year in the calculations.  Thus, year 0
     *	  (a leap year) is included only if years > 0.
     */

    days += years * 365 + ((years+3) / 4);
	
    if (years <= 100)
	;
    else if (years <= 200)		/* no leap 3 of 4 cent. marks	*/
	days -= 1;
    else if (years <= 300)
	days -= 2;
    else
	days -= 3;
    days -= 138062;			/* 1600 -> 1978			*/
    date->ds_Days  = days + tm->tm_yday;
    date->ds_Minute= tm->tm_min + tm->tm_hour * 60;
    date->ds_Tick  = tm->tm_sec * 50;
}

