/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include "phase2.h"
#include "usloss.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
int invalidArgs(int mbox_id, int msg_max_size);
void clock_handler(int code, void * dev);
void disk_handler(int code, void * dev);
void term_handler(int code, void * dev);
void wakeUpReceive(int mbox_id);
int check_io(void);
void disableInterrupts();
void enableInterrupts();
void finish();

/* -------------------------- Globals ---	---------------------------------- */

int debugflag2 = 0;
int BLOCKMECONSTANT = 22;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];
int boxID;

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...
// a process table for phase2
procStruct p2procTable[MAXPROC];
int slotsUsed;

mailSlot slottyArray[MAXSLOTS];
interruptHandler intTable[MAXHANDLERS];


/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kid_pid;
    int status;
    
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    /* test if in kernel mode; halt if in user mode */
    if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
        USLOSS_Halt(1);

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    int iter = 0;
    for(;iter < MAXMBOX; iter++){
      MailBoxTable[iter].mboxID = INACTIVE;
    }
    
    for(iter=0;iter < MAXPROC; iter++){
        p2procTable[iter].pid=-1;
        p2procTable[iter].status=-1;
    }

    slotsUsed=0;
    boxID=0;
    
    // Initialize USLOSS_IntVec and system call handlers,

    // allocate mailboxes for interrupt handlers.  Etc... 
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);
    MboxCreate(1, MAX_MESSAGE);

    // Interrupt Handlers
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clock_handler;
    USLOSS_IntVec[USLOSS_DISK_INT] = disk_handler;
    USLOSS_IntVec[USLOSS_TERM_INT] = term_handler;


    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    
    /* test if in kernel mode; halt if in user mode */
    if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
        USLOSS_Halt(1);
    
    /* Finding the first free mailbox slot in mailboxtable */
    int i;
    for(i=boxID; i<boxID+MAXMBOX; i++){
        if(MailBoxTable[i%MAXMBOX].mboxID==-1){
            boxID=i;
            MailBoxTable[i%MAXMBOX].mboxID = boxID;
            MailBoxTable[i%MAXMBOX].maxSlots = slots;
            MailBoxTable[i%MAXMBOX].usedSlots = 0;
            MailBoxTable[i%MAXMBOX].slotSize =  slot_size>MAX_MESSAGE ? MAX_MESSAGE : slot_size;
            MailBoxTable[i%MAXMBOX].firstSlot = NULL;
            MailBoxTable[i%MAXMBOX].sendList = NULL;
            MailBoxTable[i%MAXMBOX].receiveList = NULL;
            return boxID;
        }
    }
    return -1;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    
    disableInterrupts();
    /* test if in kernel mode; halt if in user mode */
    if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
        USLOSS_Halt(1);
    
    // If inactive mailbox
    if(MailBoxTable[mbox_id%MAXMBOX].mboxID==-1)
        return -1;
    // If message size too big
    if(MailBoxTable[mbox_id%MAXMBOX].slotSize<msg_size)
        return -1;
    
    if(slotsUsed > MAXSLOTS){
        USLOSS_Console("Exceeded the number of system slots allowed. Halting...\n");
        USLOSS_Halt(1);
    }
        
    
    // If no slots available, block the sending process
    if(MailBoxTable[mbox_id%MAXMBOX].usedSlots >= MailBoxTable[mbox_id%MAXMBOX].maxSlots){
        
        mailLine * new = NULL;
        new->PID = getpid();
        new->next = NULL;
        mailLine * temp = MailBoxTable[mbox_id%MAXMBOX].sendList;
        
        if(temp!=NULL){
            for(; temp->next!=NULL; temp = temp->next);
            temp->next = new;
        }
        else{
            temp = new;
        }
        
        p2procTable[getpid()%MAXPROC].status = mbox_id;
        p2procTable[getpid()%MAXPROC].pid = getpid();
        blockMe(BLOCKMECONSTANT);
    }
    
    // Check if the mailbox had been released
    if(MailBoxTable[mbox_id%MAXMBOX].mboxID== INACTIVE){
        return -3;
    }

    // Adding message to slot in mailbox
    slotPtr slot = MailBoxTable[mbox_id%MAXMBOX].firstSlot;
    for(;slot->nextSlot!=NULL;slot=slot->nextSlot);
    
    mailSlot mailSlot;
    memcpy(mailSlot.message, msg_ptr, msg_size);
    mailSlot.mboxID = mbox_id;
    mailSlot.nextSlot = NULL;
    
    slot->nextSlot = &mailSlot;
    
    // Unblock any processes waiting on receiving a message from the mailbox
    if(MailBoxTable[mbox_id%MAXMBOX].sendList!=NULL){
        unblockProc(MailBoxTable[mbox_id%MAXMBOX].sendList->PID);
        MailBoxTable[mbox_id%MAXMBOX].sendList = MailBoxTable[mbox_id%MAXMBOX].sendList->next;
    }
    // Increment amount of total system slots used
    slotsUsed++;
    
    enableInterrupts();
    
    
    return 0;
    
    
    
    /*Check for possible errors (message size too large, inactive mailbox id, etc.).
     • Return -1 if errors are found.
     • If a slot is available in this mailbox, allocate a slot from your mail slot table. MAXSLOTS determines the size of
     this array. MAX_MESSAGE determines the max number of bytes that can be held in the slot.
     • Note: if the mail slot table overflows, that is an error that should halt USLOSS.
     • Further note: for conditional send, the mail slot table overflow does not halt USLOSS.
     • Return -2 in this case.
     • Copy message into this slot. Use memcpy, not strcpy: messages can hold any type of data, not just strings.
     • Block the sender if this mailbox has no available slots. For example, mailbox has 5 slots, all of which
     already have a message.
     */
    
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
 /* test if in kernel mode; halt if in user mode */
 if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
   USLOSS_Halt(1);

 // if the arguments passed to the method are invalid, return -1
 if(invalidArgs(mbox_id, msg_size))
    return -1;
 /* disable interrupts */
 disableInterrupts();
 
 /* if the process is not able to obtain a message from the appropriate mailbox, block */
 if(MailBoxTable[mbox_id % MAXMBOX].usedSlots == 0){
   /* add the current node to the front of the receive wait list */
   mailLine tempMailLine;
   tempMailLine.PID = getpid();
   tempMailLine.next = NULL;
   if(MailBoxTable[mbox_id % MAXMBOX].receiveList == NULL){
     MailBoxTable[mbox_id % MAXMBOX].receiveList = &tempMailLine;
   }else if(MailBoxTable[mbox_id % MAXMBOX].receiveList->next == NULL){
     MailBoxTable[mbox_id % MAXMBOX].receiveList->next = &tempMailLine;
   }else{
     /* find the last MailLine object in line and append */
     mailLine * last = MailBoxTable[mbox_id % MAXMBOX].receiveList;
     while(last->next != NULL){
       last = last->next;
     }
     last->next = &tempMailLine;
   }
   /* block */
   blockMe(BLOCKMECONSTANT);
 }
 
 /* if the mailbox has since been released, return -3 */
    if(MailBoxTable[mbox_id % MAXMBOX].mboxID == INACTIVE){
 /* else obtain the message */

  void* answer;
  answer = memcpy(msg_ptr, MailBoxTable[mbox_id % MAXMBOX].firstSlot->message, msg_size);
  if(answer == NULL) return -1;
  MailBoxTable[mbox_id % MAXMBOX].firstSlot = MailBoxTable[mbox_id % MAXMBOX].firstSlot->nextSlot;
  }

  /* unblock process waiting to send, if exists */
  if(MailBoxTable[mbox_id % MAXMBOX].sendList != NULL){
  int tempPID = MailBoxTable[mbox_id % MAXMBOX].sendList->PID;
  MailBoxTable[mbox_id % MAXMBOX].sendList = MailBoxTable[mbox_id % MAXMBOX].sendList->next;
  unblockProc(tempPID);
  }
 return msg_size;
 
} /* MboxReceive */

/* returns 0 if successful, 1 if mailbox full, -1 if illegal args */
/* ------------------------------------------------------------------------
 Name - MboxCondSend
 Purpose - Conditionally send a message to a mailbox. Do not block the
 invoking process.
 Parameters - mailbox id, pointer to put data of msg, max # of bytes that
 can be received.
 Returns - -3: process was zap ’d.
 -2: mailbox full, message not sent; or no slots available in the system.
 -1: illegal values given as arguments.
 0: message sent successfully.
 Side Effects - none.
 ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void* msg_ptr, int msg_size)
{
	/* test if in kernel mode; halt if in user mode */
	if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet())) 
		USLOSS_Halt(1);

	// if the arguments passed to the method are invalid, return -1
	if(invalidArgs(mbox_id, msg_size))
	   return -1;
	// if the process is zapped, return -3
	if(isZapped()){
		USLOSS_Console("Process has been zapped\n");
		return -3;
	}
	
	/* disable interrupts */
	disableInterrupts();
	
	/* test if the mailbox is full */
	if(MailBoxTable[mbox_id % MAXMBOX].maxSlots == MailBoxTable[mbox_id % MAXMBOX].usedSlots){
		wakeUpReceive(mbox_id);
		return -2;
	}
	else{
		/* deposit the message into the mailbox */
		slotPtr temp = MailBoxTable[mbox_id % MAXMBOX].firstSlot;
		for(; temp->nextSlot != NULL; temp = temp->nextSlot);
		mailSlot newSlot;
		newSlot.message = msg_ptr;
		newSlot.mboxID = mbox_id;
		newSlot.nextSlot = NULL;
		temp->nextSlot = &newSlot;
		MailBoxTable[mbox_id % MAXMBOX].usedSlots++;
	}

	// Unblock any processes waiting on receiving a message from the mailbox
	if(MailBoxTable[mbox_id%MAXMBOX].sendList!=NULL){
		unblockProc(MailBoxTable[mbox_id%MAXMBOX].sendList->PID);
		MailBoxTable[mbox_id%MAXMBOX].sendList = MailBoxTable[mbox_id%MAXMBOX].sendList->next;
	}

	return 0;
}

/* returns 0 if successful, 1 if no msg available, -1 if illegal args */
/* ------------------------------------------------------------------------
 Name - MboxCondReceive
 Purpose - Conditionally receive a message from a mailbox. Do not block
 the invoking process.
 Parameters - mailbox id, pointer to put data of msg, max # of bytes that
 can be received.
 Returns - -3: process was zap ’d while blocked on the mailbox
 -2: mailbox empty, no message to receive
 -1: illegal values given as arguments; or, message sent is
 too large for receiver’s buffer (no data copied in this case).
 >=0: the size of the message received.
 Side Effects - none.
 ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void* msg_ptr, int msg_max_size)
{
	/* kernel mode test; halt if in user mode */
	if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
		USLOSS_Halt(1);

	// if the arguments passed to the method are invalid, return -1
	if(invalidArgs(mbox_id, msg_max_size))
	   return -1;
	// if the process is zapped, return -3
	if(isZapped()){
		USLOSS_Console("Process has been zapped\n");
		return -3;
	}

	/* disable interrupts */
	disableInterrupts();

	slotPtr temp;

	/* test if the mailbox is empty */
	if(MailBoxTable[mbox_id % MAXMBOX].usedSlots == 0)
		return -2;
	else{
		temp = MailBoxTable[mbox_id % MAXMBOX].firstSlot;
		/* if the size of the message stored in the slot is too large, return -1 */
		if(temp->msg_size > msg_max_size){
			return -1;
		}
		/* copy the message */
		msg_ptr = temp->message;
		/* "free" the slot in the mailbox */
		MailBoxTable[mbox_id % MAXMBOX].firstSlot = MailBoxTable[mbox_id % MAXMBOX].firstSlot->nextSlot;
		MailBoxTable[mbox_id % MAXMBOX].usedSlots--;
	}
	return temp->msg_size;
}

/* ------------------------------------------------------------------------
 Name - MboxRelease
 Purpose - Releases a previously created mailbox. Any process waiting on
 the mailbox should be zap ’d. Note, however, that zap ’ing does
 not quite work. It would work for a high priority process releasing
 low priority processes from the mailbox, but not the other way
 around. You will need to devise a different means of handling
 processes that are blocked on a mailbox being released.
 Essentially, you will need to have a blocked process return -3
 from the send or receive that caused it to block. You will need
 to have the process that called MboxRelease unblock all the
 blocked processes. When each of these processes awake from the
 block_me call inside send or receive, they will need to “notice”
 that the mailbox has been released...
 Parameters - mailbox id, pointer to put data of msg, max # of bytes that
 can be received.
 Returns - -3: process was zap ’d while releasing the mailbox
 -1: the mailboxID is not a mailbox that is in use
 0: successful completion
 Side Effects - none.
 ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id)
 {
     
     /* test if in kernel mode; halt if in user mode */
     if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
         USLOSS_Halt(1);

     // If mailbox not in use
     if(MailBoxTable[mbox_id%MAXMBOX].mboxID == INACTIVE)
         return -1;
     
     // Zapping all blocked processes on mailbox DOES NOT WORK
     mailLine * temp;
     for(temp = MailBoxTable[mbox_id%MAXMBOX].sendList; temp!=NULL; temp = temp->next){
         unblockProc(temp->PID);
     }
     for(temp = MailBoxTable[mbox_id%MAXMBOX].receiveList; temp!=NULL; temp = temp->next){
         unblockProc(temp->PID);
     }
     
     if(isZapped()){ 
         return -3;
     }
     
     // Releasing the mailbox
     MailBoxTable[mbox_id%MAXMBOX].mboxID=INACTIVE;
     
     return 0;
}

 // a helper method which checks if the specified id and max size pass muster
int invalidArgs(int mbox_id, int msg_max_size)
{
  if(MailBoxTable[mbox_id%MAXMBOX].mboxID == INACTIVE){
	  return 1;
  }
  else if(msg_max_size < MAX_MESSAGE){
	  return 1;
  }
  else
	  return 0;
}

void wakeUpReceive(int mbox_id){
	// Unblock any processes waiting on receiving a message from the mailbox
	if(MailBoxTable[mbox_id%MAXMBOX].receiveList!=NULL){
		unblockProc(MailBoxTable[mbox_id%MAXMBOX].receiveList->PID);
		MailBoxTable[mbox_id%MAXMBOX].receiveList = MailBoxTable[mbox_id%MAXMBOX].receiveList->next;
	}
}


/*-----------------------------------------------------------------------
Name - waitdevice
Purpose - Do a receive operation on the mailbox associated with the given
unit of the device type. The device types are defined in usloss.h . The
appropriate device mailbox is sent a message every time an interrupt is
generated by the I/O device, with the exception of the clock device which
should only be sent a message every 100,000 time units (every 5 interrupts).
This routine will be used to synchronize with a device driver process in the
next phase. waitdevice returns the device’s status register in *status .
Returns - -1: the process was zap’ d while waiting
0: successful completion
Side Effects - none.
----------------------------------------------------------------------- */
extern int waitdevice(int type, int unit, int *status)
{
	/* kernel mode test; halt if in user mode */
	if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
		USLOSS_Halt(1);

	/* perform a receive on the mailbox type */
	if(type < USLOSS_CLOCK_DEV || type > USLOSS_TERM_DEV){
		USLOSS_Halt(1);
	}
	MboxReceive(MailBoxTable[(type)%MAXMBOX].mboxID, status, MAX_MESSAGE);

	/* if it was zapped while it was blocked, return -1 */
	if(isZapped()){
		return -1;
	}

	return 0;
}

void clock_handler(int interruptNum, void * unit)
{
    int status;
    int valid;
    // Error checking if the device really is the clock device
    
    // Clock Handling Things
    if(USLOSS_Clock() - readCurStartTime() > 80000){
        timeSlice();
    }
    
    // Getting the device status register info
    valid = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, (long) unit, &status);
    
    if (valid != USLOSS_DEV_OK){
        USLOSS_Console("clock_handler: USLOSS_DeviceInput returned a bad value.\n");
        USLOSS_Halt(1);
    }
    // Conditionally send to clock mailbox every 5th interrupt.
    if(interruptNum!=0 && interruptNum % 5 == 0){
        MboxCondSend(CLOCKMBOX, &status, sizeof(int));
    }
    }

// accepts interrupt signals from DIS
void disk_handler(int code, void * dev)
{
    void * stats = NULL;
	/* obtain terminal status register */
	USLOSS_DeviceInput(USLOSS_DISK_DEV, (long) dev, stats);
	/* write the terminal status register to the correct mailbox */
	switch((long) dev){
		/* write to the appropriate mailbox and wake up any waiting processes */
		case 0:
			MboxCondSend(DISKZEROMBOX, stats, 1);
			break;
		case 1:
			MboxCondSend(DISKONEMBOX, stats, 1);
			break;
		default:
			USLOSS_Halt(1);
	}
}

void term_handler(int code, void * dev)
{
    void * stats = NULL;
	/* obtain terminal status register */
	USLOSS_DeviceInput(USLOSS_TERM_DEV, (long)dev, stats);
	/* write the terminal status register to the correct mailbox */
	switch((long)dev){
		/* write to the appropriate mailbox and wake up any waiting processes */
		case 0:
			MboxCondSend(TERMZEROMBOX, stats, 1);
			break;
		case 1:
			MboxCondSend(TERMONEMBOX, stats, 1);
			break;
		case 2:
			MboxCondSend(TERMTWOMBOX, stats, 1);
			break;
		case 3:
			MboxCondSend(TERMTHREEMBOX, stats, 1);
			break;
		default:
			USLOSS_Halt(1);
	}
}


int check_io(void){
    int i;
    for(i=0;i<MAXMBOX;i++){
        if(MailBoxTable[i].receiveList!=NULL || MailBoxTable[i].sendList!=NULL)
            return 1;
    }
    return 0;
}

/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    /* turn the interrupts OFF iff we are in kernel mode */
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
    /* We ARE in kernel mode */
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */

/*
 * Enables the interrupts.
 */
void enableInterrupts()
{
    /* turn the interrupts ON iff we are in kernel mode */
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("enable interrupts\n");
        USLOSS_Halt(1);
    } else
    /* We ARE in kernel mode */
        USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
} /* enableInterrupts */

