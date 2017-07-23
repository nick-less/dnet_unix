
/*
 *  NET.C
 *
 *	DNET (c)Copyright 1988, Matthew Dillon, All Rights Reserved
 *
 *  NetWork raw device interface.  Replace with whatever interface you
 *  want.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <sys/stat.h>
#include "dnet.h"
#include "subs.h"
#include "net.h"


void gwritetcp(int fd, register unsigned char *buf, register long bytes) ;

static int socket_fd;
static struct sockaddr_in server;

void RcvInt (int sig, siginfo_t *info, void *ucontext) {
    int n = recv(socket_fd, RcvBuf + RcvData , RCVBUF - RcvData , 0);

    int i;
    extern int errno;
    char c = *(RcvBuf + RcvData);

    errno = 0; 
    if (n >= 0){
	    RcvData += n;
    }
    if (n <= 0) {  /* disallow infinite fast-timeout select loops */
	    RExpect = 0;
    }       
    if (DDebug) {
	    printf("read(%d,%d) %02x\n", n, errno, c);
    }
}
int setasync(int sockfd)
{
    int n;

    if (fcntl(sockfd, F_SETOWN, getpid()) < 0)
        return(-1);
    n = 1;
    if (ioctl(sockfd, FIOASYNC, &n) < 0)
        return(-1);
    return(0);
}

int clrasync(int sockfd)
{
    int n;

    n = 0;
    if (ioctl(sockfd, FIOASYNC, &n) < 0)
        return(-1);
    return(0);
}


int NetOpen(void) {

    socket_fd = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_fd == -1) {
        printf("Could not create socket\n");
    }
         
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 1234 );
 
    //Connect to remote server
    if (connect(socket_fd , (struct sockaddr *)&server , sizeof(server)) < 0) {
        printf("connect error\n");
        return -1;
    }
     
    printf("Connected\n");
    setasync(socket_fd);
    signal(SIGIO, RcvInt);
    int opt = 1;
    ioctl(socket_fd, FIONBIO, &opt);

    return socket_fd;
}

void NetClose(void) {

    close(socket_fd);
    
}


void NetWrite(unsigned char *buf, long bytes) {
    if (DDebug) {
    	fprintf(stderr, "NETWRITE %p %ld\n", buf, bytes);
    }
    gwritetcp(socket_fd, buf, bytes);
}

void gwritetcp(int fd, register unsigned char *buf, register long bytes) {
    register long n;
    while (bytes) {
        n = write(fd, buf, bytes);
        if (n > 0) {
            bytes -= n;
            buf += n;
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
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
        if (errno == EPIPE) {
            return;
        }
        dneterror("gwrite");
    }
}


