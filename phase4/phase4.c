
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
void syscall_disk_size(sysargs *args);
void syscall_disk_write(sysargs *args);
static void nullsys3(sysargs *args_ptr);
int addSleepList(int pid, list list);
void* popList(list list, request_list requestList);

/* ------------------------------------------------------------------------
   Global Variables
   ----------------------------------------------------------------------- */
static int running; /*semaphore to synchronize drivers and start3*/

//static struct driver_proc Driver_Table[MAXPROC];    // Driver Table
process ProcTable[MAXPROC];                         // Process Table
list SleepingProcs;                                 // Linked list of sleeping processes
request_list DiskRequests;                                  // Linked list of disk requests


static int diskpids[DISK_UNITS];
static int num_tracks[DISK_UNITS];              // Array to store number of tracks for each disk unit
static int diskSemaphores[DISK_UNITS];          // Array to hold the disk semaphores

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
    /*for (int i = 0; i < MAXSYSCALLS; i++)
    {
        // Initialize every system call handler as nullsys3;
        sys_vec[i] = nullsys3;
    }*/
    // Initialize each system call handler that is required individually
    sys_vec[SYS_SLEEP]     = syscall_sleep;
    sys_vec[SYS_DISKREAD] = syscall_disk_read;
    // disk write
    sys_vec[SYS_DISKSIZE] = syscall_disk_size; // disk size
    sys_vec[SYS_DISKWRITE] = syscall_disk_write;
    sys_vec[SYS_DISKREAD] = syscall_disk_read;


    /* Initialize the phase 4 process table */
    for (int i = 0; i < MAXPROC; ++i)
    {
        // Initialize mailboxes
        ProcTable[i].privateMbox = MboxCreate(0,0);     // Initialize private mailboxes
        ProcTable[i].pid = i;
        ProcTable[i].sleepSem = semcreate_real(0);
    } 

    // Initialize linked list of sleeping processes
    SleepingProcs = (struct list *)malloc(sizeof(struct list));
    SleepingProcs->pHead = NULL;
    SleepingProcs->pTail = NULL;
    SleepingProcs->count = 0;
    SleepingProcs->type = TYPE_LIST;

    // Initialize linked list of disk requests
    DiskRequests = (struct request_list *)malloc(sizeof(struct request_list));
    DiskRequests->pHead = NULL;
    DiskRequests->pTail = NULL;
    DiskRequests->count = 0;
    DiskRequests->type = TYPE_REQUEST_LIST;

    // Initialize disk semaphores
    for (int i = 0; i < DISK_UNITS; ++i)
    {
        diskSemaphores[i] = semcreate_real(0);
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
    for (i = 0; i < DISK_UNITS; i++) {
        char buf[32]; // stack size?
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
    join(&status); /* for the Clock Driver */   // Deadlock occurs here
    semv_real(diskSemaphores[0]);   // Wake up the disk drivers
    semv_real(diskSemaphores[1]);   // Semv or semfree_real?
    join(&status);
}


/*----------------------------------- Clock -----------------------------------

------------------------------------------------------------------------------- */
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
    while(! is_zapped()) 
    {
        result = waitdevice(CLOCK_DEV, 0, &status);
        if (result != 0) 
        {
            return 0;
        }
        /*
        * Compute the current time and wake up any processes
        * whose time has come.
        */
        process *current = SleepingProcs->pHead; 
        while (SleepingProcs->count > 0 && current != NULL)
        {
            double curTime = (double)sys_clock() / 1000000; // Convert time (in microseconds) to seconds
            double doneSleeping = curTime - current->sleepEndTime;          // Check how long process has slept for
            if (doneSleeping >= 0)              // Check if we've slept the required time
            {
                // Wake up process
                popList(SleepingProcs, NULL); // If the next process to wake up is not the head, we need to change this function to pop specific item
                semv_real(current->sleepSem);
                break;
            }
        }
    }
    return 0;
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
    args->arg4 = (int)0;
    return;
    
}

int sleep_real(int seconds)
{   
    int pid = getpid();
    process *current = &ProcTable[pid % MAXPROC];

    // process requests a delay for x seconds (how many microseconds in a second)
    current->status= STATUS_SLEEPING;
    current->sleepStartTime = (double)sys_clock() / 1000000;    // Store the time (in seconds) the process started sleeping
    current->sleepEndTime = current->sleepStartTime + seconds; // Store the time (in seconds) that the process should wake up

    // process puts itself on a queue - process can block on it's own private mbox
    addSleepList(pid, SleepingProcs);   // Add process to the queue based on sleepTime

    //MboxReceive(current->privateMbox, NULL, 0); // Block on private mailbox
    semp_real(current->sleepSem);   // Block on sleep semaphore

    // clock driver checks on each clock interrupt to see which process(es) to wake up
        // driver calls waitdevice to wait for interrupt handler
        // compute current time and wake up any process whose time has come

    // After wakeup... check if been zapped?
    if (is_zapped())
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

/* ----------------------- Disk ---------------------------------

----------------------------------------------------------------- */

static int
DiskDriver(char *arg)
{
    int pid = getpid();
    int unit = atoi(arg);
    device_request my_request;
    int result;
    int status;
    int trackCount;

    disk_request current_req;

    /* Get the number of tracks for this disk */
    my_request.opr  = DISK_TRACKS;
    my_request.reg1 = &trackCount;

    result = device_output(DISK_DEV, unit, &my_request); 

    if (result != DEV_OK) {
        console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
        console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
        halt(1);
    }

    waitdevice(DISK_DEV, unit, &status);

    num_tracks[unit] = trackCount;
    //more code 
    
    semv_real(running); // Signal start3 that we're running

    while (ProcTable[pid].isZapped != 1) 
    {
        // wait for a request
        semp_real(diskSemaphores[unit]);

        // TODO: Check out and work on requests
        // Get next request
        pdisk_request pRequest = DiskRequests->pHead;

        // Is this a valid request?
        if (pRequest == NULL)
        {
            return 0;
        }

        // move to correct track
        my_request.opr  = DISK_SEEK;
        my_request.reg1 = pRequest->track_start;
        my_request.reg2 = pRequest->sector_start;

        result = device_output(DISK_DEV, unit, &my_request); 

        if (result != DEV_OK) {
            console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
            console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
            halt(1);
        }

       waitdevice(DISK_DEV, unit, &status); // wait for some reason
    

        // we are at the right track, read sector
        // TODO: READ SECTOR or write sector
        my_request.opr = pRequest->operation;   // Set request operation
        result = device_output(DISK_DEV, unit, &my_request); // Execute it

        // Check it was good
        if (result != DEV_OK) {
                console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
                console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
                halt(1);
            }
        waitdevice(DISK_DEV, unit, &status); // wait for some reason

        pRequest->num_sectors++;

        // are we done with this request??
        pRequest = (pdisk_request)popList(NULL, DiskRequests);


        return 0;
    }

    return 0;
}

void syscall_disk_read(sysargs *args)
{
    int pid = getpid(); // Get PID
    pdisk_request request = &ProcTable[pid % MAXPROC].diskRequest;  // Store request in process's request

    // parse arguments
    request->disk_buf = args->arg1;             // Input buffer
    request->num_sectors = (int)args->arg2;   // Number of sectors to read
    request->track_start = (int)args->arg3;        // First track to read
    request->sector_start = (int)args->arg4;  // First sector to read
    request->unit = (int)args->arg5;         // Disk unit

    // add this operation to the queue (disk driver will execute it)
    addRequestList(DiskRequests, request);

    // return to disk driver
    semv_real(diskSemaphores[request->unit]); 
}

void syscall_disk_write(sysargs *args)
{
    int pid = getpid(); // Get PID
    pdisk_request request = &ProcTable[pid % MAXPROC].diskRequest;  // Store request in process's request

    // parse arguments
    request->disk_buf = args->arg1;             // Input buffer
    request->num_sectors = (int)args->arg2;   // Number of sectors to read
    request->track_start = (int)args->arg3;        // First track to read
    request->sector_start = (int)args->arg4;  // First sector to read
    request->unit = (int)args->arg5;         // Disk unit

    // add this operation to the queue (disk driver will execute it)
    addRequestList(DiskRequests, request);

    // return to disk driver
    semv_real(diskSemaphores[request->unit]);

    // did we have to seek?
    semp_real(diskSemaphores[request->unit]);
}

void syscall_disk_size(sysargs *args)
{
    int unit = (int)args->arg1;
    int sectorSize;
    int sectorsPerTrack;
    int trackCount;

    // error checking
    if (unit > 1 || unit < 0)
    {
        // return error
    }

    sectorSize = DISK_SECTOR_SIZE;
    sectorsPerTrack = DISK_TRACK_SIZE;
    trackCount = num_tracks[unit];

    args->arg1 = (int)sectorSize;
    args->arg2 = (int)sectorsPerTrack;
    args->arg3 = (int)trackCount;
    args->arg4 = (int)0; 

}



/* -------------------------------- Other Functions ----------------------------------

-------------------------------------------------------------------------------------- */

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
   Name - check_kernel_mode()
   Purpose - To check if the current execution mode is kernel mode and halt otherwise.
   Parameters - char string[] - String to print in case of error.
   Returns - None.
   Side Effects - Halts the machine if not in kernel mode.
   ----------------------------------------------------------------------- */
void check_kernel_mode(char string[])
{
    // Get the current process status register (PSR)
    int currentPsr = psr_get();

    // if the kernel mode bit is not set, then halt
    if ((currentPsr & PSR_CURRENT_MODE) == 0)
    {
        // not in kernel mode
        console("%s, Kernel mode expected, but function called in user mode.\n", string);
        halt(1);
    }
}

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
    sleeping_process->pid = pid;
    int isHead;

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
            if (sleeping_process->sleepEndTime < current->sleepEndTime)
            {
                // If process to add will finish sooner, add it before this node
                if (current->pPrev == NULL || current == list->pHead) // If existing sleeper was the head
                {
                    list->pHead = sleeping_process; // Make the list's head the new sleeper
                    isHead = 1;
                }
                sleeping_process->pPrev = current->pPrev;   // Set the new sleeper's previous link
                current->pPrev = sleeping_process;          // Set the existing sleeper's previous link
                sleeping_process->pNext = current;          // Set the new sleeper's next link
                if (!isHead)
                {
                    sleeping_process->pPrev->pNext = sleeping_process;  // Set the previous existing sleeper's next link
                }
                list->count++;
                return 1;
            }
            current = current->pNext;   // Continue to the next node
        }
        // If we get here, we must add our node to the tail
        list->pTail->pNext = sleeping_process;
        sleeping_process->pPrev = list->pTail;
        list->pTail = sleeping_process;
        list->count++;
        return 1;
    }
}

/* ------------------------------------------------------------------------
   Name - addRequestList
   Purpose - Adds the disk request to the request list, sorted from lowest 
    track to highest track
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int addRequestList(request_list list, pdisk_request request)
{
    int isHead = 0;

    // Add item sorted for elevator algorithm
    // Sort items from bottom of the disk to top of the disk, 
        // Traverse list using pNext or pPrev according to the direction

    // Is there a process on the list?
    if (list->pHead == NULL)    // List is empty
    {
        // Initialize this list
        request->pNext = NULL;
        request->pPrev = NULL;
        list->pTail = request;
        list->pHead = request;
        list->count++;
        return 1;
    }

    // List has member(s)
    else   
    {
        // Traverse the nodes
        pdisk_request current = list->pHead;
        while (current!=NULL)
        {
            // Compare start tracks
            if (request->track_start < current->track_start)
            {
                // If request to add has a lower track
                if (current->pPrev == NULL || current == list->pHead) // If existing request was the head
                {
                    list->pHead = request; // Make the list's head the new request
                    isHead = 1;
                }
                request->pPrev = current->pPrev;   // Set the new request's previous link
                current->pPrev = request;          // Set the existing request's previous link
                request->pNext = current;          // Set the new request's next link
                if (!isHead)
                {
                    request->pPrev->pNext = request;  // Set the previous existing request's next link
                }
                list->count++;
                return 1;
            }
            // Check if track is the same
            else if (request->track_start == current->track_start)
            {
                // Compare start sectors
                if (request->sector_start < current->sector_start)
                {
                    // If request to add has a lower sector
                    if (current->pPrev == NULL || current == list->pHead) // If existing request was the head
                    {
                        list->pHead = request; // Make the list's head the new request
                        isHead = 1;
                    }
                    request->pPrev = current->pPrev;   // Set the new request's previous link
                    current->pPrev = request;          // Set the existing request's previous link
                    request->pNext = current;          // Set the new request's next link
                    if (!isHead)
                    {
                        request->pPrev->pNext = request;  // Set the previous existing request's next link
                    }
                    list->count++;
                    return 1;
                }
            }
            current = current->pNext;   // Continue to the next node
        }
        // If we get here, we must add our node to the tail
        list->pTail->pNext = request;
        request->pPrev = list->pTail;
        list->pTail = request;
        list->count++;
        return 1;
    }

}

/* ------------------------------------------------------------------------
   Name - popList
   Purpose - Removes the first process from the list.
   Parameters - list - the list pointer to the list to pop the process off of.
   Returns - the process / request pointer if a process is successfully removed, 0 if the waiting list is empty.
   Side Effects - Decreases the count of waiting processes for the mailbox.
   ----------------------------------------------------------------------- */
void* popList(list list, request_list requestList)
{
    check_kernel_mode("popWaitList\n");
    process *poppedProc;
    disk_request *diskRequest;

    if (list == NULL)   // Request List
    {

        disk_request *poppedProc = requestList->pHead; // get the head of the list (oldest item)

        // Check if this is the only item
        if (requestList->count == 1)
        {
            requestList->pHead = NULL; // make head NULL
            requestList->pTail = NULL; // make tail NULL
            requestList->count--;      // decrement count
            return poppedProc;           // return
        }

        // Update the head to the next process
        requestList->pHead = poppedProc->pNext;           

        // Update head/tail pointers
        if (requestList->pHead == NULL)
        {
            requestList->pTail = NULL; // If the head becomes NULL (no more items), update the tail as well
        }
        else
        {
            requestList->pHead->pPrev = NULL; // Update the new head's previous pointer to NULL
        }

        // Update the popped process's pointers
        if (poppedProc->pNext != NULL)
        {
            poppedProc->pNext->pPrev = NULL; // Update the next process's previous pointer to NULL
        }

        // Decrement the count of processes in the list
        requestList->count--;

        return poppedProc;
    }
    else    // Regular process
    {   
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
        if (poppedProc->status == STATUS_SLEEPING)
        {
            poppedProc->status = 0;
            poppedProc->sleepStartTime = 0;
            poppedProc->sleepEndTime = 0;
            numSleepingProc--;
        }

        return poppedProc;
    }
}