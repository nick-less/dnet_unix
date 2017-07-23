
/*
 *  GETFILES.C	    V1.30
 *
 *  DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved.
 *
 *  GETFILES [-dlocaldir] [-c] file/dir file/dir file/dir
 *
 *  -dlocaldir	local directory to place files
 *
 *  -c		Continue from where you left off before.  Files that already
 *		exist on the local machine will not be re-transfered.  This
 *		will also re-start in the middle of a file that was
 *		partially transfered previously.
 *
 *		This command assumes the file(s) on both machines have not
 *		been modified since the first attempt.	No other checking
 *		is done at the moment.
 */

#include <stdio.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>    
#include <string.h>    
#include <stdlib.h>    

#include "../server/servers.h"
#include "../lib/dnetlib.h"

#include "getfiles.h"

char Buf[1024];

short ContMode;
char  *NetId;
long  Chan = -1;


int main(int ac,char *av[]) {
    HDR Hdr;
    short error;

    {
	char *ldir = ".";
	struct stat stat;

	ac = DoOption(ac, av, "N%sd%sc", &NetId /*, &ldir, &ContMode*/);
	if (ac <= 1) {
	    puts("GETFILES [-Nnetid -dlocaldir -c] remotefile/dir ...");
	    fail(22);
	}
	if (chdir(ldir) < 0) {
	    mkdir(ldir, 0777);
	    if (chdir(ldir) < 0) {
		printf("Unable to CD or make local directory: \"%s\"\n",ldir);
		fail(21);
	    }
	}
    }
    Chan = DOpen(NetId, PORT_GFILECOPY, 126, -80);
    if (Chan < 0) {
	puts("Unable to connect");
	fail(20);
    }
    error = WriteHeader('H', "Hello, getfiles client V1.30", 0);
    if (error)
	fail(LostChannel());
    switch(ReadHeader(&Hdr)) {
    case -1:
	fail(LostChannel());
    case 'H':
	printf("%s\n", Hdr.Str);
	break;
    }
    {
	register short i;
	long val;

	for (i = 1; i < ac; ++i) {
	    short error;

	    error = WriteHeader('G', av[i], 0);
	    if (error)
		fail(LostChannel());
	    switch(ReadHeader(&Hdr)) {
	    case -1:
		fail(LostChannel());
	    case 'N':
		printf("Remote error on %s: %s\n", av[i], Hdr.Str);
		break;
	    case 'F':
		error = CheckNoPath(Hdr.Str);
		if (!error) {
		    char svpath[1024];
		    getwd(svpath);
		    error = GetFile(&Hdr, 0);
		    chdir(svpath);
		}
		break;
	    case 'D':
		error = CheckNoPath(Hdr.Str);
		if (!error) {
		    char svpath[1024];
		    getwd(svpath);
		    error = GetDir(&Hdr, 0);
		    chdir(svpath);
		}
		break;
	    case 'S':
		printf("Access Violation: %s\n", Hdr.Str);
		break;
	    default:
		error = UnknownCmd(&Hdr);
		break;
	    }
	    if (error)
		fail(error);
	}
	if (!error) {
	    error = WriteHeader('E', "bye", 0);
	    if (error)
		fail(LostChannel());
	}
    }
    fail(0);
}

void fail(int code) {
    if (Chan >= 0) {
		close(Chan);
	}
    exit(code);
}

int CheckNoPath(register char *str)  {
    while (*str) {
	if (*str == '/' || *str == ':') {
	    puts("SECURITY ALERT: Illegal path spec received");
	    return(40);
	}
	++str;
    }
    return(0);
}

int LostChannel(void) {
    puts("DATA CHANNEL LOST");
    return(10);
}

int UnknownCmd(HDR *hdr) {
    printf("Unrecognized command code: %02x\n", hdr->Cmd);
    return(100);
}

/*
 *  retrieve a file.  If ContMode set and file exists, try to append to
 *  it.
 */



int GetFile(HDR *hdr, int stab) {
    int fd = -1;
    long pos = 0;
    short error = 0;

    printf("%s%-20s ", NSpaces(stab), hdr->Str);
    fflush(stdout);
    if (ContMode) {
	if ((fd = open(hdr->Str, O_WRONLY)) >= 0) {    /*  already exists  */
	    long len;

	    len = lseek(fd, 0L, 2);
	    if (len > hdr->Val) {
		close(fd);
		printf("Cont Error, local file is larger than remote!: %s\n",
		   hdr->Str
		);
		puts("(not downloaded)");
		return(0);
	    }
	    if (len == hdr->Val) {
		close(fd);
		if ((error = WriteHeader('S', NULL, 0)))
		    return(LostChannel());
		puts("HAVE IT, SKIP");
		return(0);
	    }
	    printf("(HAVE %ld/%ld) ", len, hdr->Val);
	    hdr->Val -= len;		/*  that much less  */
	    pos = len;			/*  start at offset */
	}
    }
    if (fd < 0) {
	fd = open(hdr->Str, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
	    error = WriteHeader('N', "open error", 0);
	    printf("Unable to open %s for output\n", hdr->Str);
	    if (error)
		return(LostChannel());
	    return(1);
	}
    }
    error = WriteHeader('Y', NULL, pos);    /*  yes, gimme gimme    */

    /*
     *	Retrieve the data
     */

    if (!error) {
	register long left = hdr->Val;
	register long cnt = pos;
	long total = pos + left;

	printf("             ");
	while (left) {
	    register long n = (left > sizeof(Buf)) ? sizeof(Buf) : left;
	    printf("%s%8ld/%8ld", BSSTR, cnt, total);
	    fflush(stdout);
	    if (ggread(Chan, Buf, n) != n) {
		error = 5;
		break;
	    }
	    if (write(fd, Buf, n) != n) {
		puts("Local Write failed!");
		error = 6;
		break;
	    }
	    left -= n;
	    cnt += n;
	}
	printf("%s%8ld/%8ld  %s", BSSTR, cnt, total,
	    ((cnt == total) ? "OK" : "INCOMPLETE")
	);
    }
    puts("");
    if (error) {
	fchmod(fd, 0222);
	close(fd);
	return(LostChannel());
    }
    close(fd);
    return(error);
}

/*
 *  Retrieve a directory.  Create it if necessary.
 */

int GetDir(HDR *hdr, int stab) {
    short error = 0;
    long dirlock;
    static HDR Hdr;	    /*	note: static */
    char svpath[1024];

    printf("%s%-20s(DIR)\n", NSpaces(stab), hdr->Str);
    getwd(svpath);
    if (chdir(hdr->Str) < 0) {
	mkdir(hdr->Str, 0777);
	if (chdir(hdr->Str) < 0) {
	    error = WriteHeader('N', "couldn't create", 0);
	    printf("Unable to create local directory: %s\n", hdr->Str);
	    if (error)
		return(LostChannel());
	    return(1);
	}
    }
    error = WriteHeader('Y', NULL, 0);  /*  yes, gimme gimme    */
    while (!error) {
	switch(ReadHeader(&Hdr)) {
	case -1:
	    error = 1;
	    break;
	case 'E':                   /*  end of directory    */
	    chdir(svpath);
	    return(0);
	    break;
	case 'F':
	    error = CheckNoPath(Hdr.Str);
	    if (!error) {
		char svpath2[1024];
		getwd(svpath2);
		error = GetFile(&Hdr, stab + 4);
		chdir(svpath2);
	    }
	    break;
	case 'D':
	    error = CheckNoPath(Hdr.Str);
	    if (!error) {
		char svpath2[1024];
		getwd(svpath2);
		error = GetDir(&Hdr, stab + 4);
		chdir(svpath2);
	    }
	    break;
	case 'S':
	    printf("Access Violation: %s\n", Hdr.Str);
	    break;
	case 'N':
	    printf("REMOTE ERROR: %s\n", Hdr.Str);
	    error = 10;
	    break;
	default:
	    error = UnknownCmd(&Hdr);
	    break;
	}
    }
    chdir(svpath);
    return(LostChannel());
}

int WriteHeader(char c, char *str, long len) {
    ubyte sl;

    if (str == NULL)
	str = "";
    sl = strlen(str);

    if (gwrite(Chan, &c, 1) < 0) {
		return(1);
	}
    if (gwrite(Chan, (char*)&sl,1) < 0) {
		return(1);
	}
    if (gwrite(Chan, str, sl) != sl) {
		return(1);
	}
    len = htonl68(len);
    if (gwrite(Chan, (char*)&len, 4) != 4) {
		return(1);
	}
    return(0);
}

int ReadHeader(HDR *hdr) {
    ubyte sl;
    ubyte cmd;

    hdr->Cmd = -1;
    if (ggread(Chan, (char*)&cmd, 1) != 1)
	return(-1);
    if (ggread(Chan, (char*)&sl, 1) != 1)
	return(-1);
    if (sl >= sizeof(hdr->Str)) {
	puts("Software error: received file name length too long");
	return(-1);
    }
    if (ggread(Chan, hdr->Str, sl) != sl)
	return(-1);
    hdr->Str[sl] = 0;
    if (ggread(Chan, (char*)&hdr->Val, 4) != 4)
	return(-1);
    hdr->Val = ntohl68(hdr->Val);
    hdr->Cmd = cmd;
    return(hdr->Cmd);
}

char * NSpaces(short n) {
    static char Buf[128];
    static short in = 0;
    static short last;

    if (in == 0) {
	register short i;
	in = 1;
	for (i = 0; i < sizeof(Buf); ++i)
	    Buf[i] = ' ';
    }
    Buf[last] = ' ';
    if (n < 127)
	Buf[n] = 0;
    last = n;
    return(Buf);
}

