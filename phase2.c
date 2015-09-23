/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...




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
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc... 

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

}
