
#define DEBUG2 1

#define TRUE 1
#define FALSE 0

#define MAXHANDLERS 1

#define INACTIVE 0
#define ACTIVE 1


typedef struct mailSlot mailSlot;
typedef struct mailSlot * slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
typedef void (*interruptHandler)(int);
<<<<<<< HEAD
=======
typedef struct procStruct procStruct;
>>>>>>> 32a77f78bfb9385c0b47df827b70973257b595db

struct mailbox {
    int       mboxID;
    // other items as needed...
    int maxSlots;
    int usedSlots;
    int slotSize;
    slotPtr firstSlot;
    /* a list of processes waiting for slots */
    procStruct * waitList;
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
    char * message;
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
    procStruct * nextProc;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};
