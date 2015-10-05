
#define DEBUG2 1

#define TRUE 1
#define FALSE 0

#define CLOCKMBOX 0
#define DISKZEROMBOX 1
#define DISKONEMBOX 2
#define TERMZEROMBOX 3
#define TERMONEMBOX 4
#define TERMTWOMBOX 5
#define TERMTHREEMBOX 6

#define MAXHANDLERS 1

#define INACTIVE -1


typedef struct mailSlot mailSlot;
typedef struct mailSlot * slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
typedef void (*interruptHandler)(int);
typedef struct procStruct procStruct;

struct mailbox {
    int       mboxID;
    // other items as needed...
    int maxSlots;
    int usedSlots;
    int slotSize;
    slotPtr firstSlot;
    /* a list of processes waiting for slots */
    mailLine * receiveList;
    mailLine * sendList;
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
    int msg_size;
    char message [MAX_MESSAGE];
    slotPtr nextSlot;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

struct procStruct{
    int pid;
    int status;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};
