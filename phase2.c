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
void sys_handler(int code, void * args);
int check_io(void);
void disableInterrupts();
void enableInterrupts();
int addMessage(int mbox_id, void *msg_ptr, int msg_size);
void nullSys(systemArgs *args);
extern int waitDevice(int type, int unit, int *status);
mailLine * getWaiter();
extern int waitdevice(int type, int unit, int *status);

/* -------------------------- Globals ---	---------------------------------- */

int debugflag2 = 0;
int BLOCKMECONSTANT = 22;

// the mail boxes 
struct mailbox MailBoxTable[MAXMBOX];
int boxID;

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...
int slotsUsed;

interruptHandler intTable[MAXHANDLERS];

/* an array of waiting processes */
mailLine waitLine[MAXPROC];

/* Syscall handler vector */
void (*sys_vec[MAXSYSCALLS])(systemArgs *args);

/* global array of slots */
mailSlot slots[MAXSLOTS];
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
        MailBoxTable[iter].head = NULL;
    }
    
    for(iter=0;iter<MAXSLOTS;iter++){
        slots[iter].status=INACTIVE;
    }
    
    for(iter=0;iter<MAXPROC;iter++){
        waitLine[iter].PID=INACTIVE;
    }
    // Initialize syscall vector
    for(iter=0;iter<MAXSYSCALLS;iter++){
        sys_vec[iter]=nullSys;
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

    // Interrupt Handlers
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clock_handler;
    USLOSS_IntVec[USLOSS_DISK_INT] = disk_handler;
    USLOSS_IntVec[USLOSS_TERM_INT] = term_handler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = sys_handler;


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
    
    /* test if number of slots too large or too small */
    if(slots > MAXSLOTS || slots < 0)
       	return -1;

    /* test if slot size is too large or too small */
    if(slot_size > MAX_MESSAGE || slot_size < 0)
    	return -1;

    /* Finding the first free mailbox slot in mailboxtable */
    int i;
    for(i=boxID; i<boxID+MAXMBOX; i++){
        if(MailBoxTable[i%MAXMBOX].mboxID==INACTIVE){
        	if (DEBUG2 && debugflag2)
        	        USLOSS_Console("MboxCreate(): FOUND A BOX: %d\n", boxID);
            boxID=i;
            MailBoxTable[i%MAXMBOX].mboxID = boxID;
            MailBoxTable[i%MAXMBOX].maxSlots = slots;
            MailBoxTable[i%MAXMBOX].usedSlots = 0;
            MailBoxTable[i%MAXMBOX].slotSize =  slot_size>MAX_MESSAGE ? MAX_MESSAGE : slot_size;
            MailBoxTable[i%MAXMBOX].head = NULL;
            MailBoxTable[i%MAXMBOX].tail = NULL;
            MailBoxTable[i%MAXMBOX].waitList = NULL;
            return boxID % MAXMBOX;
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
    
    if (DEBUG2 && debugflag2)
        USLOSS_Console("MboxSend(): starting\n");
    
    disableInterrupts();
    /* test if in kernel mode; halt if in user mode */
    if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
        USLOSS_Halt(1);
    
    // If inactive mailbox
    if(MailBoxTable[mbox_id%MAXMBOX].mboxID==INACTIVE)
        return -1;

    // If message size too big
    if(MailBoxTable[mbox_id%MAXMBOX].slotSize<msg_size)
        return -1;
    if(slotsUsed > MAXSLOTS){
        USLOSS_Console("Exceeded the number of system slots allowed. Halting...\n");
        USLOSS_Halt(1);
    }
    
    // Check for WaitList
    struct mailbox * target = &MailBoxTable[mbox_id % MAXMBOX];
    // Unblock process that is receiveBlocked and deliver it's message to the mailbox right away
    if(target->waitList!=NULL && target->usedSlots == 0){
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): Unblocking Proc %d.\n", target->waitList->PID);
        //addMessage(mbox_id, MailBoxTable[mbox_id%MAXMBOX].waitList->msg, MailBoxTable[mbox_id%MAXMBOX].waitList->msgSize);
        /* place the message in the address of the waiting process */
        memcpy(target->waitList->msg, msg_ptr, msg_size);
        target->waitList->msgSize=msg_size;
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): after memcpy, the waitListMsg is : %s\n", target->waitList->msg);
        
        unblockProc(target->waitList->PID);
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): after unblockproc\n");
        return 0;
    }
        
    // If no slots available, block the sending process
    if(MailBoxTable[mbox_id%MAXMBOX].usedSlots >= MailBoxTable[mbox_id%MAXMBOX].maxSlots){
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): No slots available in mailbox: %d\n", mbox_id);
        int i;
        // Finding an open slot in waitLine & adding the wait-ee's information into it.
        for(i=0;i<MAXPROC;i++){
            if(waitLine[i].PID==INACTIVE){
                if (DEBUG2 && debugflag2)
                    USLOSS_Console("MboxSend(): Found an open slot in waitLine at index %d.\n", i);
                waitLine[i].PID=getpid();
                waitLine[i].next=NULL;
                memcpy(waitLine[i].msg, msg_ptr, msg_size);
                waitLine[i].msgSize=msg_size;
                if(MailBoxTable[mbox_id%MAXMBOX].waitList == NULL){
                    MailBoxTable[mbox_id%MAXMBOX].waitList = &waitLine[i];
                    if (DEBUG2 && debugflag2)
                        USLOSS_Console("MboxSend(): First sender in the waitList: %s\n", MailBoxTable[mbox_id%MAXMBOX].waitList->msg);
                }
                // Assigning the latest wait-ee to the next in line in the waitLine table.
                else{
                    mailLine * temp;
                    for(temp = MailBoxTable[mbox_id%MAXMBOX].waitList; temp->next!=NULL; temp = temp->next);
                    temp->next = &waitLine[i];
                    if (DEBUG2 && debugflag2)
                        USLOSS_Console("MboxSend(): First sender in the waitList: %s\n", MailBoxTable[mbox_id%MAXMBOX].waitList->msg);
                }
                break;
            }
        }
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxSend(): BLOCKED\n");
        blockMe(BLOCKMECONSTANT);
        // Check if the mailbox had been released
        waitLine[i].PID=INACTIVE;
        if(MailBoxTable[mbox_id%MAXMBOX].mboxID== INACTIVE){
            return -3;
        }
        else{
            return 0;
        }
    }
    
    // If there was no receiveBlocked processes, add the message like normal
    else{
        // Adding message to slot in mailbox
        addMessage(mbox_id, msg_ptr, msg_size);
    }
    
    // Check if the mailbox had been released
    if(MailBoxTable[mbox_id%MAXMBOX].mboxID== INACTIVE){
        return -3;
    }
     
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
   Returns - actual size of msg if successful, -1 if invalid args, -3 if
   	   	   	 Mbox was zapped or released before receive occurred
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
	int recMsgSize;
    if (DEBUG2 && debugflag2)
        USLOSS_Console("MboxReceive(): Starting\n");

	/* test if in kernel mode; halt if in user mode */
	if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
		USLOSS_Halt(1);

	/* if the arguments passed to the method are invalid, return -1 */
	if(invalidArgs(mbox_id, msg_size))
		return -1;
	/* disable interrupts */
	disableInterrupts();

	struct mailbox * target = &MailBoxTable[mbox_id % MAXMBOX];
	if (DEBUG2 && debugflag2)
		        USLOSS_Console("MboxReceive(): Checkpoint1\n");
	/* if the process is not able to obtain a message from the appropriate mailbox, block */
	if(target->usedSlots == 0){
		if (DEBUG2 && debugflag2)
			        USLOSS_Console("MboxReceive(): Checkpoint3\n");
		/* add the current node to the rear of the waitList */
		mailLine * tempWaiter;
		if(target->waitList == NULL){
			if (DEBUG2 && debugflag2)
				        USLOSS_Console("MboxReceive(): Checkpoint4\n");
			target->waitList = getWaiter();
			tempWaiter = target->waitList;
		}
		/* find a free waitList object in the array and append to waitList */
		else{
			if (DEBUG2 && debugflag2)
				        USLOSS_Console("MboxReceive(): Checkpoint5\n");
			tempWaiter = target->waitList;
			/* find the end of the waitList */
			for(;tempWaiter->next != NULL; tempWaiter = tempWaiter->next);
			/* append a new waitList object */
			tempWaiter->next = getWaiter();
			tempWaiter = tempWaiter->next;
		}
		tempWaiter->PID = getpid();
		tempWaiter->next = NULL;
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxReceive(): BLOCKED\n");

		/* block on receive */
		blockMe(BLOCKMECONSTANT);

		/* on being unblocked, remove self from waitList and return message length */
        if (DEBUG2 && debugflag2){
            USLOSS_Console("MboxReceive(): back from block\n");
            USLOSS_Console("MboxReceive(): message: %s\n", tempWaiter->msg);
        }
        
        memcpy(msg_ptr,tempWaiter->msg,tempWaiter->msgSize);

        
		tempWaiter->PID = INACTIVE;
		target->waitList = tempWaiter->next;
		if(DEBUG2 && debugflag2)
			USLOSS_Console("MboxReceive(): %s\n%d\n", tempWaiter->msg, tempWaiter->msgSize);



		recMsgSize = tempWaiter->msgSize;
	}
	/* else obtain the message */
	else{
		if (DEBUG2 && debugflag2)
			        USLOSS_Console("MboxReceive(): Checkpoint6\n");
		if (DEBUG2 && debugflag2)
			USLOSS_Console("MboxReceive(): get message: %s\n", target->head->message);
		recMsgSize = target->head->msg_size;
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxReceive(): recMsgSize = %d\n", recMsgSize);

		void* answer;
		answer = memcpy(msg_ptr, target->head->message, msg_size);
		if(answer == NULL && msg_size != 0)
			return -1;
		/* take the empty slot off of the list and decrement the number of messages */
		target->head->status = INACTIVE;
		target->head = target->head->nextSlot;
		target->usedSlots--;
		slotsUsed--;
	}

	/* if send-blocked processes exist, move message into slot and unblock waiting process */
	if(target->waitList != NULL && target->usedSlots != 0){
		if (DEBUG2 && debugflag2)
					USLOSS_Console("MboxReceive(): Checkpoint2\n");
		int waitPID = target->waitList->PID;
		addMessage(mbox_id, target->waitList->msg, target->waitList->msgSize);
		target->waitList->PID = INACTIVE;
		target->waitList = target->waitList->next;
		recMsgSize = target->head->msg_size;
		unblockProc(waitPID);
	}

	/* if the mailbox has since been released, return -3 */
	if(target->mboxID == INACTIVE){
		return -3;
	}else
		return recMsgSize;
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

	/* if the arguments passed to the method are invalid, return -1 */
	if(invalidArgs(mbox_id, msg_size))
	   return -1;

	// if the process is zapped, return -3
	if(isZapped()){
		USLOSS_Console("Process has been zapped\n");
		return -3;
	}
	
	/* disable interrupts */
	disableInterrupts();
	
	struct mailbox * target = &MailBoxTable[mbox_id % MAXMBOX];
	/* if there is a process blocked on receive from the mailbox */
	if(target->waitList != NULL && target->head == NULL){
		if (DEBUG2 && debugflag2){
			USLOSS_Console("MboxCondSend(): a process is receiveBlocked\n");
			USLOSS_Console("MboxCondSend(): message = %d\n", *(int *)msg_ptr);
			USLOSS_Console("MboxCondSend(): size = %d\n", msg_size);
		}
		int waitPID = target->waitList->PID;
		memcpy(target->waitList->msg, msg_ptr, msg_size);
        if (DEBUG2 && debugflag2){
            USLOSS_Console("MboxCondSend(): waitListMsg: %d\n", target->waitList->msg);
        }
		target->waitList->PID = INACTIVE;
		target->waitList = target->waitList->next;
		unblockProc(waitPID);
	}
	/* if the mailbox is unable to accept further messages return -2 */
	else if(target->usedSlots >= target->maxSlots || slotsUsed>=MAXSLOTS){
			return -2;
	}else{
		/* deposit the message into the mailbox */
        addMessage(mbox_id, msg_ptr, msg_size);
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
	struct mailbox * target = &MailBoxTable[mbox_id % MAXMBOX];
	/* test if the mailbox is empty */
	if(target->usedSlots == 0)
		return -2;
	else{
		temp = target->head;
		/* if the size of the message stored in the slot is too large, return -1 */
		if(temp->msg_size > msg_max_size){
			return -1;
		}
		/* copy the message */
		memcpy(msg_ptr, temp->message, temp->msg_size );
		/* "free" the slot in the mailbox */
		target->head = target->head->nextSlot;
		target->usedSlots--;
		slotsUsed--;
	}

	/* if send-blocked processes exist, move message into slot and unblock waiting process */
	if(target->waitList != NULL){
		int waitPID = target->waitList->PID;
		addMessage(mbox_id, target->waitList->msg, target->waitList->msgSize);
		target->waitList->PID = INACTIVE;
		target->waitList = target->waitList->next;
		unblockProc(waitPID);
	}
	if(isZapped() || MailBoxTable[mbox_id % MAXMBOX].mboxID == INACTIVE){
		return -3;
	}else
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
     
     // Releasing the mailbox
     MailBoxTable[mbox_id%MAXMBOX].mboxID=INACTIVE;
     
     // Zapping all blocked processes on mailbox DOES NOT WORK
     mailLine * temp;
     for(temp = MailBoxTable[mbox_id%MAXMBOX].waitList; temp!=NULL; temp = temp->next){
         unblockProc(temp->PID);
     }
     
     if(isZapped()){ 
         return -3;
     }
     
     
     
     return 0;
}

 // a helper method which checks if the specified id and max size pass muster
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
extern int waitDevice(int type, int unit, int *status)
{
    int valid;
	/* kernel mode test; halt if in user mode */
	if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
		USLOSS_Halt(1);

	/* perform a receive on the mailbox type */
	if(type < USLOSS_CLOCK_DEV || type > USLOSS_TERM_DEV){
		USLOSS_Halt(1);
	}
	MboxReceive(MailBoxTable[(type)%MAXMBOX].mboxID, status, MAX_MESSAGE);

    // Getting the device status register info
    valid = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, (long) unit, status);

    if (valid != USLOSS_DEV_OK){
        USLOSS_Console("clock_handler: USLOSS_DeviceInput returned a bad value.\n");
        USLOSS_Halt(1);
    }
	/* if it was zapped while it was blocked, return -1 */
	if(isZapped()){
		return -1;
	}

	return 0;
}

void clock_handler(int devNum, void * unit)
{
    static int interruptNum = 1;
    int status = 0;
    // Error checking if the device really is the clock device
    
    if (DEBUG2 && debugflag2){
        USLOSS_Console("clock_handler(): called for the %d time\n", interruptNum);
    }
    



    // Conditionally send to clock mailbox every 5th interrupt.
    if(interruptNum!=0 && interruptNum % 5 == 0){
    	if (DEBUG2 && debugflag2){
			USLOSS_Console("clock_handler(): sending message = %d\n", status);
		}
        MboxCondSend(CLOCKMBOX, &status, sizeof(int));
    }
    interruptNum++;
    
    // Clock Handling Things
    if(USLOSS_Clock() - readCurStartTime() > 80000){
        timeSlice();
    }
}

// accepts interrupt signals from DISK device
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

/* handler for terminal interrupts */
void term_handler(int code, void * dev)
{
    int stats;
	/* obtain terminal status register */
	USLOSS_DeviceInput(USLOSS_TERM_DEV, (long)dev, &stats);
	stats = USLOSS_TERM_STAT_CHAR(stats);
	/* write the terminal status register to the correct mailbox */
	switch((long)dev){
		/* write to the appropriate mailbox and wake up any waiting processes */
		case 0:
			MboxCondSend(TERMZEROMBOX, &stats, 1);
			break;
		case 1:
			MboxCondSend(TERMONEMBOX, &stats, 1);
			break;
		case 2:
			MboxCondSend(TERMTWOMBOX, &stats, 1);
			break;
		case 3:
			MboxCondSend(TERMTHREEMBOX, &stats, 1);
			break;
		default:
			USLOSS_Halt(1);
	}
}


void sys_handler(int code, void * args){
    if (DEBUG2 && debugflag2){
        USLOSS_Console("sys_handler(): System call number is: %d\n", *(int*)args);
    }
    
    if(*(int*)args>=0 && *(int*)args<MAXSYSCALLS){
        systemArgs newArgs;
        newArgs.number=*(int*)args;
        sys_vec[*(int*)args](&newArgs);
    }
    else{
        USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n", *(int*)args);
        USLOSS_Halt(1);
    }
    
}


int check_io(void){
    int i;
    for(i=0;i<MAXMBOX;i++){
        if(MailBoxTable[i].waitList!=NULL && MailBoxTable[i].waitList->PID != INACTIVE){
        	return 1;
        }
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

/*
 Adds a message into a slot for the specified mailbox.
 
 Halts if # of slots used > allowed for the system.
 Returns:
    -1 if the msg size is greater than allowed for the mailbox || Mailbox is inactive || Mailbox is full
    0 if succeeded
 */
int addMessage(int mbox_id, void *msg_ptr, int msg_size){
    
    // Check if there are any slots left in the system
    if(slotsUsed>=MAXSLOTS){
        USLOSS_Console("addMessage(): # of messages in slots exceed the maximum allowed. Halting...\n");
        USLOSS_Halt(1);
    }
    
    // Check if mailbox is full
    if(MailBoxTable[mbox_id%MAXMBOX].usedSlots >= MailBoxTable[mbox_id%MAXMBOX].maxSlots){
        return -1;
    }
    
    int i;
    // Finding an empty slot to put into
    for(i=0;i<MAXSLOTS;i++){
        if(slots[i].status == INACTIVE){
            // Checking Msg size & mailbox active
            if(MailBoxTable[mbox_id%MAXMBOX].slotSize < msg_size || MailBoxTable[mbox_id%MAXMBOX].mboxID == INACTIVE){
                return -1;
            }
            memcpy(slots[i].message, msg_ptr, msg_size);
            slots[i].msg_size = msg_size;
            slots[i].mboxID = mbox_id;
            slots[i].status = i;
            slots[i].nextSlot = NULL;
            break;
        }
    }
    
    // Rejigger the head & tail pointers
    if(MailBoxTable[mbox_id%MAXMBOX].head == NULL){
        MailBoxTable[mbox_id%MAXMBOX].head = &slots[i];
        MailBoxTable[mbox_id%MAXMBOX].tail = &slots[i];
    }
    else{
        MailBoxTable[mbox_id%MAXMBOX].tail->nextSlot = &slots[i];
        MailBoxTable[mbox_id%MAXMBOX].tail = &slots[i];
    }
    MailBoxTable[mbox_id%MAXMBOX].usedSlots++;
    slotsUsed++;
    
    return 0;
}

void nullSys(systemArgs *args){
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
}
/* returns an inactive mailLine pointer */
mailLine * getWaiter(){
	int iter;
	for(iter = 0; iter < MAXPROC; iter++){
		if(waitLine[iter].PID == INACTIVE){
			waitLine[iter].PID = iter;
			return &waitLine[iter];
		}
	}
	return NULL;
}

int invalidArgs(int mbox_id, int msg_max_size)
{
  if(MailBoxTable[mbox_id%MAXMBOX].mboxID == INACTIVE){
	  return 1;
  }
  else if(msg_max_size > MAX_MESSAGE){
	  return 1;
  }
  else
	  return 0;
}

