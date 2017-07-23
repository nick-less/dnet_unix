
/*
 *  DNET.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *	Handles action on all active file descriptors and dispatches
 *	to the proper function in FILES.C
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/un.h>

#include "dnet.h"
#include "files.h"
#include "net.h"

#define SASIZE(sa)      (sizeof(sa)-sizeof((sa).sa_data)+strlen((sa).sa_data))


void setlistenport(char *remotehost);
void handle_child(void);
char * showselect(fd_set *ptr) ;
void do_restart(void);
void do_netreset(void);

struct sigaction *oldsighup;

void handle_child(void) {
    union wait stat;
    struct rusage rus;
    while (wait3(&stat, WNOHANG, &rus) > 0);
}

char * showselect(fd_set *ptr) {
    static char buf[FD_SETSIZE+32];
    short i;

    for (i = 0; i < FD_SETSIZE; ++i) {
		buf[i] = (FD_ISSET(i, ptr)) ? '1' : '0';
    }
    buf[i] = 0;
    return(buf);
}

 void loganddie(int sig, siginfo_t *info, void *ucontext) {
    fflush(stderr);
    fprintf(stderr, "\nHUPSIGNAL\n");
    perror("HUP, last error:");
    fprintf(stderr, "Last select return:\n");
    fprintf(stderr, "  %s\n", showselect(&Fdread));
    fprintf(stderr, "  %s\n", showselect(&Fdwrite));
    fprintf(stderr, "  %s\n", showselect(&Fdexcept));
    fprintf(stderr, "RcvData = %ld\n", RcvData);
    fprintf(stderr, "RChan/WChan = %ld/%ld\n", RChan, WChan);
    fprintf(stderr, "RPStart = %ld\n", RPStart);
    fprintf(stderr, "WPStart = %ld\n", WPStart);
    fprintf(stderr, "WPUsed = %ld\n", WPUsed);
    fprintf(stderr, "RState = %ld\n", RState);
    fflush(stderr);
    //kill(0, SIGILL);
    exit(1);
}


int main(int ac,char *av[]) {
    long sink_mask, dnet_mask;
    ubyte notdone;
    char local_dir[MAXPATHLEN];
    struct passwd pw_info;

    if (write(0, "", 0) < 0) {
	perror("write");
	exit(1);
    }

    if (getenv("DNETDIR")) {
	strcpy(local_dir, getenv("DNETDIR"));
	if (chdir(local_dir)) {
	    fprintf(stderr, "Unable to chdir to DNETDIR: %s\n", local_dir);
	    exit(1);
	}
	// freopen("DNET.LOG", "w", stderr);
//	setlinebuf(stderr);
    } else {
	pw_info = *getpwuid(getuid());
	strcpy(local_dir, pw_info.pw_dir);
	strcat(local_dir, "/.dnet");
	if (chdir(local_dir)) {
	    mkdir(local_dir, 0700);
	    if (chdir(local_dir)) {
		fprintf(stderr, "Unable to create dir %s\n", local_dir);
		exit(1);
	    }
	}


	//freopen("DNET.LOG", "w", stderr);

    }


    fprintf(stderr, "DNet startup\n");
    fprintf(stderr, "Log file placed in %s\n", local_dir);

    /*signal(SIGINT, SIG_IGN);*/
    signal(SIGPIPE, SIG_IGN);
    /*signal(SIGQUIT, SIG_IGN);*/
    signal(SIGCHLD, handle_child);
    signal(SIGHUP, loganddie);

	sigaction(SIGHUP, loganddie, oldsighup);


    bzero(Pkts,sizeof(Pkts));

    setlistenport("3");


    {
	register short i;
		for (i = 1; i < ac; ++i) {
			register char *ptr = av[i];
			if (*ptr != '-') {
			DDebug = 1;
			fprintf(stderr, "Debug mode on\n");
			setlinebuf(stderr);
			setlinebuf(stdout);
			continue;
			}
			++ptr;
			switch(*ptr) {
			case 'B':
			break;
			case 'm':	/* Mode7 */
			Mode7 = atoi(ptr + 1);
			if (Mode7 == 0)
				fprintf(stderr, "8 bit mode selected\n");
			else
				fprintf(stderr, "7 bit mode selected\n");
			break;
			default:
			fprintf(stderr, "Unknown option: %c\n", *ptr);
			printf("Unknown option: %c\n", *ptr);
			exit(1);
			}
		}
    }

    NewList(&TxList);

    Fdperm[0] = 1;
    Fdstate[0] = RcvInt;


    fprintf(stderr, "DNET RUNNING, Listenfd=%ld\n", DNet_fd);
    int fd_in = NetOpen();          /* initialize network and interrupt driven read */
    TimerOpen();        /* initialize timers                            */

    fprintf(stderr, "transport fd=%ld\n", fd_in);

    FD_SET(fd_in, &Fdread);
    FD_SET(fd_in, &Fdexcept);

    do_netreset();
    do_restart();

    notdone = 1;
    while (notdone) {
		/*
		*    MAIN LOOP.  select() on all the file descriptors.  Set the
		*    timeout to infinity (NULL) normally.  However, if there is
		*    a pending read or write timeout, set the select timeout
		*    to 2 seconds in case they timeout before we call select().
		*    (i.e. a timing window).  OR, if we are in the middle of a
		*    read, don't use descriptor 0 and timeout according to
		*    the expected read length, then set the descriptor as ready.
		*/

		fd_set fd_rd;
		fd_set fd_wr;
		fd_set fd_ex;
		struct timeval tv, *ptv;
		int err;

		fd_rd = Fdread;
		fd_wr = Fdwrite;
		fd_ex = Fdexcept;

		tv.tv_sec = 0;		/* normally wait forever for an event */
		tv.tv_usec= 0;
		ptv = NULL;
		if ((Rto_act || Wto_act)) {     /* unless timeout pending */
			ptv = &tv;
			tv.tv_sec = 2;
		}

		/*   ... or expecting data (don't just wait for one byte).
		*
		*   This is an attempt to reduce the CPU usage for the process.
		*   If we are expecting data over the serial line, then don't
		*   return from the select() even if data is available, but
		*   wait for the timeout period indicated before reading the
		*   data.  Don't wait more than 64 byte times or we may loose
		*   some data (the silo's are only so big.. like 128 bytes).
		*
		*   Currently we wait 1562uS/byte (1/10 second for 64 bytes)
		*   This number is used simply so we don't hog the cpu reading
		*   a packet.
		*/

		if (RExpect) {
			ptv = &tv;
			tv.tv_usec= 1562L * ((RExpect < 64) ? RExpect : 64);
			tv.tv_sec = 0;
			FD_CLR(fd_in, &fd_rd);
		}
		if (WReady) {	/* transmit stage has work to do */
			ptv = &tv;
			tv.tv_usec = 0;
			tv.tv_sec = 0;
		}
		err = select(FD_SETSIZE, &fd_rd, &fd_wr, &fd_ex, ptv);
		if (RExpect) {
			FD_SET(fd_in, &fd_rd);   /* pretend data ready */
		}
		if (DDebug)
			fprintf(stderr, "SERR %ld %ld %08lx %08lx\n",
			err, errno, RExpect, ptv
			);

		if (RTimedout) {
			RTimedout = 0;
			do_rto();
		}
		if (WTimedout) {
			WTimedout = 0;
			Wto_act = 0;
			do_wto();
		}
		if (err < 0) {
			if (errno == EBADF) {
			perror("select");
			dneterror(NULL);
			}
		} else {
			register short i;
			register short j;
			register long mask;

			for (i = 0; i < FD_SETSIZE/NFDBITS; ++i) {
			if (mask = fd_ex.fds_bits[i]) {
				for (j = i * NFDBITS; mask; (mask >>= 1),(++j)) {
				if (mask & 1)
					(*Fdstate[j])(2,j);
				}
			}
			if (mask = fd_wr.fds_bits[i]) {
				for (j = i * NFDBITS; mask; (mask >>= 1),(++j)) {
				if (mask & 1)
					(*Fdstate[j])(1,j);
				}
			}
			if (mask = fd_rd.fds_bits[i]) {
				for (j = i * NFDBITS; mask; (mask >>= 1),(++j)) {
				if (mask & 1)
					(*Fdstate[j])(0,j);
				}
			}
			}
		}
		if (RcvData) {
			do_rnet();
		}
		do_wupdate();
    }
    dneterror(NULL);
}

void nop(void) {
}

void do_netreset(void) {
	if (DDebug) {
		fprintf(stderr, "begin do_netreset");
	}
    register short i;
    register CHAN *ch;
    for (i = 0; i < FD_SETSIZE; ++i) {
	if (!Fdperm[i])
	    Fdstate[i] = nop;
    }
    for (i = 0, ch = Chan; i < MAXCHAN; ++i, ++ch) {
	switch(ch->state) {
	case CHAN_OPEN:
	case CHAN_LOPEN:	/*  pending on network	    */
	case CHAN_CLOSE:
	    if (ch->fd >= 0) {
		FD_CLR(ch->fd, &Fdread);
		FD_CLR(ch->fd, &Fdexcept);
		Fdstate[ch->fd] = nop;
		close(ch->fd);
		ch->fd = -1;
		ch->state = CHAN_FREE;
		ch->flags = 0;
		--NumCon;
	    }
	    ClearChan(&TxList, i, 1);
	    break;
	}
    }
    RPStart = 0;
    WPStart = 0;
    WPUsed  = 0;
    RState  = 0;
    RChan = 0;
    WChan = 0;
	if (DDebug) {
		fprintf(stderr, "end do_netreset");
	}

}

void do_restart(void) {
    WriteRestart();
    Restart = 1;
}

void setlistenport(char *remotehost) {
    static struct sockaddr_un sa;
    int s;

	setenv("DNETHOST", remotehost, 0);
    sprintf(sa.sun_path, "/tmp/DNET.%s", remotehost);


    if (DNet_fd >= 0) {
	    sprintf(sa.sun_path, "/tmp/DNET.%d", DNet_fd);
		if (unlink(sa.sun_path)) {
			fprintf(stderr, "unlink failed %d\n", errno);
		}
		Fdstate[DNet_fd] = nop;
		Fdperm[DNet_fd] = 0;
		FD_CLR(DNet_fd, &Fdread);
		FD_CLR(DNet_fd, &Fdexcept);
		close(DNet_fd);
    }

  
	if (unlink(sa.sun_path)) {
		fprintf(stderr, "unlink failed %d\n", errno);
	}
    sa.sun_family = AF_UNIX;

    s = socket(PF_UNIX, SOCK_STREAM, 0);
    /* fcntl(s, F_SETOWN, getpid()); */
    fcntl(s, F_SETFL,  FNDELAY);

	fprintf(stderr, "socket %d\n", s);


    if (bind(s, &sa,  SUN_LEN(&sa)) < 0) {
		printf("error bind %d\n", errno);
		perror("bind");
		exit(1);
    }
    if (listen(s, 5) < 0) {
		unlink(sa.sun_path);
		printf("error listen\n");
		perror("listen");
		exit(1);
    }

    DNet_fd = s;
    Fdstate[DNet_fd] = do_localopen;
    Fdperm[DNet_fd] = 1;
    FD_SET(DNet_fd, &Fdread);
    FD_SET(DNet_fd, &Fdexcept);
}

