
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <usyscall.h>
#include <provided_prototypes.h>
#include "driver.h"

static int	ClockDriver(char *);
static int	DiskDriver(char *);
void syscall_sleep(sysargs *args);
int sleep_real(int seconds);
void syscall_disk_read(sysargs *args);
static void nullsys3(sysargs *args_ptr);
int addSleepList(int pid, list list);
int popList(list list);

/* ------------------------------------------------------------------------
   Global Variables
   ----------------------------------------------------------------------- */
static int running; /*semaphore to synchronize drivers and start3*/

static struct driver_proc Driver_Table[MAXPROC];    // Driver Table
process ProcTable[MAXPROC];                         // Process Table
list SleepingProcs;                                 // Linked list of sleeping processes

static int diskpids[DISK_UNITS];



int numSleepingProc;    // Global number of sleeping processes
/* ------------------------------------------------------------------------ */


/* ------------------------------------------------------------------------
   Functions

/* ------------------------------------------------------------------------
   Name - start3()
   Purpose - 
   Parameters - char *arg - Pointer to a string argument that can be used during process startup.
   Returns - int - The termination status of the last process to finish.
   Side Effects - Initializes system resources, creates and manages processes.
   ----------------------------------------------------------------------- */
int 
start3(char *arg)
{
    char	name[128];
    char        termbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;
    /*
     * Check kernel mode here.

     */

    /* Assignment system call handlers */
    for (int i = 0; i < MAXSYSCALLS; i++)
    {
        // Initialize every system call handler as nullsys3;
        sys_vec[i] = nullsys3;
    }
    // Initialize each system call handler that is required individually
    sys_vec[SYS_SLEEP]     = syscall_sleep;
    sys_vec[SYS_DISKREAD] = syscall_disk_read;
    // disk write
    // disk size
    //more for this phase's system call handlings


    /* Initialize the phase 4 process table */
    for (int i = 0; i < MAXPROC; ++i)
    {
        // Initialize mailboxes
        ProcTable[i].startupMbox = MboxCreate(1, 0);    // Initialize startup mailboxes 
        ProcTable[i].privateMbox = MboxCreate(0,0);     // Initialize private mailboxes

        // Initialize the children list
        ProcTable[i].children = malloc(sizeof(struct list));
        ProcTable[i].children->pHead = NULL;
        ProcTable[i].children->pTail = NULL;
        ProcTable[i].children->count = 0;
    } 

    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreate_real(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
	console("start3(): Can't create clock driver\n");
	halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    semp_real(running);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */
    char buf[10]; // stack size?
    for (i = 0; i < DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        diskpids[i] = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (diskpids[i] < 0) {
           console("start3(): Can't create disk driver %d\n", i);
           halt(1);
        }
    }
    semp_real(running);
    semp_real(running);


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case names.
     */
    pid = spawn_real("start4", start4, NULL,  8 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver
    join(&status); /* for the Clock Driver */
}

static int
ClockDriver(char *arg)
{
    int result;
    int status;

    /*
     * Let the parent know we are running and enable interrupts.
     */
    semv_real(running);
    psr_set(psr_get() | PSR_CURRENT_INT);
    while(! is_zapped()) {
	result = waitdevice(CLOCK_DEV, 0, &status);
	if (result != 0) {
	    return 0;
	}
	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */
    double curTime = (double)sys_clock();
    curTime = curTime / 1,000,000;              // Convert time (in microseconds) to seconds
    process *current = SleepingProcs->pHead;
    while (current != NULL)
    {
        double sleepStartTime = current->sleepStartTime;   // Convert time to seconds
        double timeSlept = curTime - sleepStartTime;       // Check how long process has slept for
        if (timeSlept >= current->sleepTime)              // Check if we've slept the required time
        {
            // Wake up process
            popList(SleepingProcs); // TODO: new functions for managing sleeping process queue by time to sleep
            MboxCondSend(current->privateMbox, NULL, 0);
        } 
    }
    }
}

void syscall_sleep(sysargs *args)
{
    int seconds = args->arg1;
    // Check for invalid args
    if (seconds <= 0)
    {
        args->arg4 = (void *)-1;
        return;
    }
    sleep_real(seconds);
}

int sleep_real(int seconds)
{   
    int pid = getpid();
    process *current = &ProcTable[pid % MAXPROC];

    // process requests a delay for x seconds (how many microseconds in a second)
    current->sleepFlag = 1;
    current->sleepTime = seconds;
    current->sleepStartTime = (double)sys_clock() / 1,000,000;  // Store the time (in seconds) the process started sleeping

    // process puts itself on a queue - process can block on it's own private mbox
    addSleepList(pid, &SleepingProcs);   // Add process to the queue based on sleepTime

    mboxReceive(current->privateMbox, NULL, 0); // Block on private mailbox

    // clock driver checks on each clock interrupt to see which process(es) to wake up
        // driver calls waitdevice to wait for interrupt handler
        // compute current time and wake up any process whose time has come

    // After wakeup... check if been zapped?

}

static int
DiskDriver(char *arg)
{
   int unit = atoi(arg);
   device_request my_request;
   int result;
   int status;

   driver_proc_ptr current_req;

   if (DEBUG4 && debugflag4)
      console("DiskDriver(%d): started\n", unit);


   /* Get the number of tracks for this disk */
   my_request.opr  = DISK_TRACKS;
   my_request.reg1 = &num_tracks[unit];

   result = device_output(DISK_DEV, unit, &my_request);

   if (result != DEV_OK) {
      console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
      console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
      halt(1);
   }

   waitdevice(DISK_DEV, unit, &status);
   if (DEBUG4 && debugflag4)
      console("DiskDriver(%d): tracks = %d\n", unit, num_tracks[unit]);


   //more code 
    return 0;
}

void syscall_disk_read(sysargs *args)
{
    // do something
}




/* ------------------------------------------------------------------------
   Name - nullsys3()
   Purpose - Default system call handler for undefined system calls.
   Parameters - sysargs *args_ptr - Pointer to sysargs structure (not used).
   Returns - None.
   Side Effects - Terminates the calling process due to invalid system call.
   ----------------------------------------------------------------------- */
static void nullsys3(sysargs *args_ptr)
{
    // Print error message and terminate
    printf("nullsys3(): Invalid syscall %d\n", args_ptr->number);
    printf("nullsys3(): process %d terminating\n", getpid());
    terminate_real(1);
} /* nullsys3 */

/* ------------------------------------------------------------------------
   Name - addSleepList
   Purpose - Adds the current process to the specified linked list.
   Parameters - int pid: the PID of the process to add.
                list list: the list pointer to add the process to.
   Returns - 1 if the process is successfully added to the waiting list, 0 otherwise.
   Side Effects - May increase the count of the waiting processes.
   ----------------------------------------------------------------------- */
int addSleepList(int pid, list list)
{
    process *sleeping_process = &ProcTable[pid % MAXPROC];  // Get process

    // Add process to mailbox's waiting list
    if (pid == NULL)
    {
        // Invalid process pointer
        return 0;
    }

    // Is there a process on the list?
    if (list->pHead == NULL)    // List is empty
    {
        // Initialize this list
        sleeping_process->pNext = NULL;
        sleeping_process->pPrev = NULL;
        list->pTail = sleeping_process;
        list->pHead = sleeping_process;
        list->count++;
        return 1;
    }

    // List has member(s)
    else   
    {
        // Traverse the nodes
        process *current = list->pHead;
        while (current!=NULL)
        {
            // Compare first node's time left to sleep (sleepTime-sleepStartTime)
            double timeLeft = current->sleepTime - current->sleepStartTime; // Get time left to sleep
            if (sleeping_process->sleepTime < timeLeft)
            {
                // If process to add has less time left, add it before this node
                if (current->pPrev == NULL || current == list->pHead) // If existing sleeper was the head
                {
                    list->pHead = sleeping_process; // Make the list's head the new sleeper
                }
                sleeping_process->pPrev = current->pPrev;   // Set the new sleeper's previous link
                current->pPrev = sleeping_process;          // Set the existing sleeper's previous link
                sleeping_process->pNext = current;          // Set the new sleeper's next link
                sleeping_process->pPrev->pNext = sleeping_process;  // Set the previous existing sleeper's next link
                list->count++;
                return 1;
            }
            current = current->pNext;   // Continue to the next node
        }
        // If we get here, we must add our node to the tail
        list->pTail->pNext = sleeping_process;
        sleeping_process->pPrev = list->pTail;
        list->pTail = sleeping_process;
        return 1;
    }
}

/* ------------------------------------------------------------------------
   Name - popList
   Purpose - Removes the first process from the list.
   Parameters - list - the list pointer to the list to pop the process off of.
   Returns - 1 if a process is successfully removed, 0 if the waiting list is empty.
   Side Effects - Decreases the count of waiting processes for the mailbox.
   ----------------------------------------------------------------------- */
int popList(list list)
{
    check_kernel_mode("popWaitList\n");

    // Check if list is empty
    if (list->count == 0)
    {
        return NULL;
    }

    // Get the oldest item and replace list's head
    process *poppedProc = list->pHead; // get the head of the list (oldest item)
    // Check if this is the only item
    if (list->count == 1)
    {
        list->pHead = NULL; // make head NULL
        list->pTail = NULL; // make tail NULL
        list->count--;      // decrement count
        return 1;           // return
    }

    // Update the head to the next process
    list->pHead = poppedProc->pNext;           

    // Update head/tail pointers
    if (list->pHead == NULL)
    {
        list->pTail = NULL; // If the head becomes NULL (no more items), update the tail as well
    }
    else
    {
        list->pHead->pPrev = NULL; // Update the new head's previous pointer to NULL
    }

    // Update the popped process's pointers
    if (poppedProc->pNext != NULL)
    {
        poppedProc->pNext->pPrev = NULL; // Update the next process's previous pointer to NULL
    }

    // Decrement the count of processes in the list
    list->count--;

    // Decrement global count of sleeping processes and reset sleep flag if this is a sleeping process
    if (poppedProc->sleepFlag == 1)
    {
        poppedProc->sleepFlag = 0;
        poppedProc->sleepStartTime = 0;
        poppedProc->sleepTime = 0;
        numSleepingProc--;
    }

    return 1;
}