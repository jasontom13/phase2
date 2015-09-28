
#define DEBUG2 1

#define TRUE 1
#define FALSE 0

#define MAXHANDLERS 1


typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
typedef void (*interruptHandler)(int)

struct mailbox {
    int       mboxID;
    // other items as needed...
    int numSlots;
    int slotSize;
    struct mailSlot * slotPtr;
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
    int   isActive;
    char * message[MAX_MESSAGE];
    struct mailSlot * nextSlot;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};
