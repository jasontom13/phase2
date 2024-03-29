
/* A test of waitDevice for a terminal.  Can be used to test other
 * three terminals as well.
 */

#include <stdio.h>
#include <strings.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>

int XXp1(char *);
int XXp3(char *);
char buf[256];

int start2(char *arg)
{
    int kid_status, kidpid;
    long control = 0;

    USLOSS_Console("start2(): started\n");

    control = USLOSS_TERM_CTRL_RECV_INT(control);

    USLOSS_Console("start2(): calling USLOSS_DeviceOutput to enable receive ");
    USLOSS_Console("interrupts, control = %d\n", control);

    USLOSS_DeviceOutput(USLOSS_TERM_DEV, 1, (void *)control);

    kidpid = fork1("XXp1", XXp1, NULL, 2 * USLOSS_MIN_STACK, 3);

    kidpid = join(&kid_status);
    USLOSS_Console("start2(): joined with kid %d, status = %d\n",
                   kidpid, kid_status);

    quit(0);
    return 0; /* so gcc will not complain about its absence... */
} /* start2 */


int XXp1(char *arg)
{
    int result, status;

    USLOSS_Console("XXp1(): started, calling waitDevice for terminal 1\n");

    result = waitDevice(USLOSS_TERM_DEV, 1, &status);
    USLOSS_Console("XXp1(): after waitDevice call\n");

    if ( result == -1 ) {
        USLOSS_Console("XXp1(): got zap'd result from waitDevice() call. ");
        USLOSS_Console("Should not have happened!\n");
    }
    else if ( result == 0 )
        USLOSS_Console("XXp1(): status = %d\n", status);
    else
        USLOSS_Console("XXp1(): got %d instead of -1 or 0 from waitDevice\n",
                     result);

    USLOSS_Console("XXp1(): receive status for terminal 1 = %d\n",
                   USLOSS_TERM_STAT_RECV(status));
    USLOSS_Console("XXp1(): character received = %c\n",
                   USLOSS_TERM_STAT_CHAR(status));

    quit(-3);
    return 0; /* so gcc will not complain about its absence... */
} /* XXp1 */
