

void RcvInt (int sig, siginfo_t *info, void *ucontext) ;

int NetOpen(void);
void NetClose(void);
void NetWrite(unsigned char *buf, long bytes);

