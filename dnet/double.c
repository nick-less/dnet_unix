
/*
 *  DOUBLE
 *
 *  DOUBLE DNETDIRA DNETDIRB
 *
 *  Run dnet using a pipe (for testing)
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

main(ac, av, envp)
char *av[];
char **envp;
{
    int files[2];
    short i;
    char buf[256];
    char cmd[256];

    if (ac != 3) {
	puts("double dnetdira dnetdirb");
	exit(1);
    }

    for (i = 0; envp[i]; ++i) {
	if (strncmp(envp[i], "DNETDIR=", 8) == 0)
	    break;
    }
    if (envp[i] == NULL) {
	puts("must set a dummy DNETDIR enviroment variable");
	exit(1);
    }


    if (socketpair(AF_UNIX, SOCK_STREAM, 0, files) < 0) {
	perror("socketpair");
	exit(1);
    }

    envp[i] = buf;
    if (fork() == 0) {
	int fd = open("d1.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (dup2(files[0], 0) < 0)
	    perror("dup2-1");
	dup2(fd, 1);
	dup2(fd, 2);
	if (write(0, "", 0) < 0) {
	    perror("write");
	    exit(1);
	}
        sprintf(buf, "DNETDIR=%s", av[1]);
	execlp("dnet", "dnet", "debug", NULL);
	perror("exec");
	exit(1);
    }
    if (fork() == 0) {
	int fd = open("d2.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (dup2(files[1], 0) < 0)
	    perror("dup2-1");
	dup2(fd, 1);
	dup2(fd, 2);
	close(files[0]);
	close(files[1]);
	if (write(0, "", 0) < 0) {
	    perror("write");
	    exit(1);
	}
        sprintf(buf, "DNETDIR=%s", av[2]);
	execlp("dnet", "dnet", "debug", NULL);
	perror("exec");
	exit(1);
    }
    close(files[0]);
    close(files[1]);
    {
	int pid;
        while ((pid = wait(0)) > 0)
	    printf("pid %d exit\n", pid);
    }
}

