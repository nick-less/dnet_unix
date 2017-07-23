
/*
 *  PUTFILES.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *  Download one or more files from the remote computer
 *
 *  PUTFILES file/dir1 file/dir2 ... file/dirN
 *
 *  placed in directory server ran from on remote host.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <string.h>
#include "../server/servers.h"
#include "../lib/dnetlib.h"

char Buf[1024];

typedef struct stat STAT;

int putname(int chan, char *file);
int putfile(int chan, char *file);
int writehdr(int chan, unsigned char c, char *name, long len);
int writehdr_nc(int chan, unsigned char c, char *name, long len);


int main(int ac, char *av[]) {
    long chan;
    long n, len, orig;
    long fh;
    short i, j;
    char fn;

    if (ac == 1) {
	puts("putfiles V1.00 (c)Copyright 1987, Matthew Dillon, All Rights Reserved");
	puts("putfiles file/dir file/dir .....");
	exit(1);
    }

    chan = DOpen(NULL, PORT_FILECOPY, -80, 126);
    if (chan < 0) {
		puts("Unable to connect");
		exit(1);
    }
    ggread(chan, &fn, 1);
    if (fn != 'Y') {
		puts("Remote Server Software Error");
		close(chan);
    }
    for (i = 1; i < ac; ++i) {
		if (strncmp(av[i], "-d", 2) == 0) {/*-ddir or -d dir*/
			char *dir = av[i]+2;
			if (*dir == 0 && i+1 < ac) {
				++i;
				dir = av[i];
			}
			if (writehdr_nc(chan, 'C', dir, 0) != 'Y') {
				puts ("unable to go to specified remote directory");
				break;
			}
		} else {
			if (putname(chan, av[i]) < 0)
			break;
		}
    }
    printf("\nclosing... ");
    fflush(stdout);
    close(chan);
    puts("done");
}

int putname(int chan, char *file) {
    STAT sstat;
    char svdir[256];
    int ret;

    printf("%-20s ", file);
    if (stat(file, &sstat) < 0) {
		puts("NOT FOUND");
		return(1);
    }
    if (sstat.st_mode & S_IFDIR) {
	DIR *dir;
	struct direct *de;

	getwd(svdir);
	puts("DIR");
	chdir(file);
	if (writehdr(chan, 'X', file, 0) != 'Y') {
	    puts("Remote unable to make directory");
	    goto f1;
	}
	if ((dir = opendir("."))) {
	    while ((de = readdir(dir)))  {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name,"..")==0)
		    continue;
		if (putname(chan, de->d_name) < 0) {
		    ret = -1;
		    break;
		}
	    }
	}
	writehdr(chan, 'Y', "?", 0);
f1:
	chdir(svdir);
    } else {
	ret = putfile(chan, file);
    }
    return(ret);
}

int putfile(int chan, char *file) {
    int fd = open(file, O_RDONLY);
    long n, r, len;
    long ttl = 0;
    char co;

    fflush(stdout);
    if (fd < 0) {
		puts("FILE NOT FOUND");
		return(0);
    }
    len = ttl = lseek(fd, 0, 2);
    lseek(fd, 0, 0);
    if (writehdr(chan, 'W', file, len) != 'Y') {
		puts("REMOTE UNABLE TO ACCEPT FILE");
		close(fd);
		return(0);
    }
    printf("%8ld/%-8ld", ttl - len, ttl);
    while (len) {
		fflush(stdout);
		r = (len > sizeof(Buf)) ? sizeof(Buf) : len;
		n = read(fd, Buf, r);
		if (n != r) {
			puts("Local File error");
			close(fd);
			return(-1);
		}
		if (gwrite(chan, (char*) &Buf, n) != n) {
			puts("Remote error");
			close(fd);
			return(-1);
		}
		len -= n;
		printf("\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010");
		printf("%8ld/%-8ld", ttl - len, ttl);
    }
    close(fd);
    if (len) {
		puts("REMOTE ERROR");
		return(-1);
    }
    printf("  Queued, waiting... ");
    fflush(stdout);
    ggread(chan, &co, 1);
    if (co != 'Y') {
		puts("Remote Server Software Error");
		return(-1);
    }
    puts("OK");
    return(0);
}

int writehdr(int chan, unsigned char c, char *name, long len)  {
    short i;
    for (i = strlen(name) - 1; i >= 0; --i) {
		if (name[i] == '/' || name[i] == ':')
			break;
    }
    name += i + 1;
    return(writehdr_nc(chan, c, name, len));
}

int writehdr_nc(int chan, unsigned char c, char *name, long len) {
    gwrite(chan, (char*) &c, 1);
    c = strlen(name) + 1;
    gwrite(chan, (char*) &c, 1);
    gwrite(chan, name, c);
    len = htonl68(len);
    gwrite(chan, (char*) &len, 4);
    if (ggread(chan, (char*) &c, 1) == 1) {
		return(c);
	}
    return(-1);
}

