
#define DEBUG2 1

#define TRUE 1
#define FALSE 0

#define CLOCKMBOX 11
#define ALARMMBOX 12
#define DISKZEROMBOX 13
#define DISKONEMBOX 14
#define TERMZEROMBOX 15
#define TERMONEMBOX 16
#define TERMTWOMBOX 17
#define TERMTHREEMBOX 18

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
    void * message;
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
