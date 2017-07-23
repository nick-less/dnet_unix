
#define ulong unsigned long
#define ubyte unsigned char
#define uword unsigned short

typedef struct {
    long    ds_Days;
    long    ds_Minute;
    long    ds_Tick;
} STAMP;

typedef struct {
    long    DirHandle;		/*  relative to directory (0=root)  */
    uword   Modes;		/*  open modes			    */
} OpOpen;

typedef struct {
    long    Handle;
    ulong   Prot;
    long    Type;
    long    Size;
    STAMP   Date;
} RtOpen;


typedef struct {
    long    Handle; 	/*  file handle to read from	    */
    long    Bytes;		/*  # of bytes to read		    */
} OpRead;

typedef struct {
    long    Bytes;		/*  < 0 == error		    */
} RtRead;

typedef struct {
    long   Handle; 	/*  file handle to read from	    */
    long    Bytes;		/*  # of bytes to read		    */
} OpWrite;

typedef struct {
    long    Bytes;		/*  < 0 == error		    */
} RtWrite;

typedef struct {
    long    Handle;
} OpClose;

typedef struct {
    long    Handle;
    long    Offset;
    long    How;
} OpSeek;

typedef struct {
    long    OldOffset;
    long    NewOffset;	    /*	-1 = error  */
} RtSeek;

typedef struct {
    long    Handle;
} OpParent;

typedef RtOpen RtParent;

typedef struct {
    long    DirHandle;
} OpDelete;

typedef struct {
    long    Error;
} RtDelete;

typedef OpDelete OpCreateDir;
typedef RtParent RtCreateDir;

typedef struct {
    long    Handle;
    long    Index;
} OpNextDir;

typedef RtOpen RtNextDir;

typedef struct {
    long    Handle;
} OpDup;

typedef RtOpen	RtDup;

typedef struct {
    long    DirHandle1;
    long    DirHandle2;
} OpRename;

typedef struct {
    long    Error;
} RtRename;

void chandler(int sig);

int OpenHandle(char *base, char *tail, int modes);
void AmigaToUnixPath(char *buf);
void ConcatPath(char *s1, char *s2, char *buf);
char * FDName(int h);
char *TailPart(char *path);
void CloseHandle(int h);
void NFs(int chan);

int DupHandle(int h);
int FDHandle(int h);
void SetDate(STAMP *date, time_t mtime) ;
