/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();
int AddToList(ProcList *list, proc_ptr process);   //added to resolve implicit declaration warning
int GetNextPid();                                  // Get next available PID
void clockHandler();                               // For Handling clock interrupts
void DebugConsole(char *format, ...);
static void check_deadlock();
int check_io();                                    // Dummy function for future phase
void checkKernelMode();                             // Check if we're in kernel mode
proc_ptr GetNextReadyProc();
void pcbClean();
int RemoveFromList(ProcList *list, proc_ptr process);   // for use in quit() function to remove children from children lists
int getpid(void);                                  // Get pid of currently running process


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

// FIRST TODO: EXAMINE THIS - the master list of all the processes that are active - any process could be ready, blocking, running
/* the process table */
proc_struct ProcTable[MAXPROC];  // Just an array that can hold 50 processes

/* Process lists  */


/* Processes */
proc_ptr Current;                      // current process ID
unsigned int next_pid = SENTINELPID;   // The next pid to be assigned
int numProc = 0;                       // Number of currently active processes

/* global variable to postpone dispatcher until first 2 processes are in the process table */
int readyToStart = 0;

/* the ready list - linked lists of ready processes, one for each priority level */
ProcList ReadyList[6];

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   for (int i = 0; i < MAXPROC; i++) {
      ProcTable[i].status = STATUS_EMPTY;   // marking each entry as empty
      ProcTable[i].pid = STATUS_UNUSED;     // mark pid as -1
      ProcTable[i].priority = STATUS_UNUSED; // mark priorty as -1
      ProcTable[i].cpu_time = STATUS_UNUSED; // mark cpu time as -1
   }

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");
   
   for (i = 0; i < sizeof(ReadyList) / sizeof(ReadyList[0]); i++) {   //I replaced 'int * ReadyList =NULL', it's not necessary since we have a Global ReadyList which is an array of 'ProcList', initialized 'ReadyList' without redeclaring it.
       ReadyList[i].pHead = NULL;
       ReadyList[i].pTail = NULL;
       ReadyList[i].count = 0;
   }

   if (DEBUG && debugflag)
       console("startup(): initialized the Ready list\n");

   /* Initialize the clock interrupt handler */
   int_vec[CLOCK_INT] = clockHandler;

   /* startup a sentinel process - the background process that runs when nobody else is ready to run. The wait room if you will (busy wait). 
      we have to have this process so that we can switch control back to it from another process that terminates, is waiting for i/o, etc. 
      lowest priority process. */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }

   // indicate we are ready to dispatch in fork1
   readyToStart = 1;
  
   /* start the test process - start1 - Make sure you're able to launch the start1 program. - start1 is the entry point for all testcases */
   DebugConsole("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1); // highest priority process (1)
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed

         Put the process in the process table and invoke the context switch
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority)
{
   int proc_slot;
   int newPid;

   DebugConsole("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   /*if ((psr_get() & PSR_CURRENT_MODE) == 0) {
      console("fork1(): not in kernel mode, halting...\n");
      halt(1);
   } */
   checkKernelMode();

   /* Return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK) {
      console("fork1(): stack size is too small, returning -2\n");
      return -2;
   }
   /* Check if Priority is out of range, Process priority 6 must have name "sentinel" */ 
   if (priority < MAXPRIORITY || (priority > MINPRIORITY && strcmp(name, "sentinel") != 0) ) {
      console("fork1(): priority out of range, returning -1\n");
      return -1;
   }
   /* Check if name is NULL */
   if (f == NULL || name == NULL) {
      console("fork1(): function or name is NULL, returning -1\n");
      return -1;
   }
   /* find an empty slot in the process table */
   newPid = GetNextPid();
   proc_slot = newPid % MAXPROC;
   /* Assign PID to process */
   ProcTable[proc_slot].pid = newPid; // Assign the next available PID
   /* Check if slot is available */
   if (proc_slot == -1) {
      console("fork1(): no empty slot available in the process table, returning -1\n");
      return -1;
   }
   /* fill-in entry in process table */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }

   strcpy(ProcTable[proc_slot].name, name);  // Put the name in the process entry (proc_struct in kernel.h)
   ProcTable[proc_slot].start_func = f;      // Assign the start function address for the process to f
   ProcTable[proc_slot].priority = priority; // Assign Process Priority
   ProcTable[proc_slot].pParent = Current;   // Store current in pParent

   if ( arg == NULL )                        // Check if arguments need to be passed
      ProcTable[proc_slot].start_arg[0] = '\0'; // If none, set the process's start_arg element to NULL
   else if ( strlen(arg) >= (MAXARG - 1) ) {    // Checks to see if it's argument is too long
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else                                      // Put the argument address (arg) into the process entry's start_arg element
      strcpy(ProcTable[proc_slot].start_arg, arg);

   // Allocates stack space for the new process
   ProcTable[proc_slot].stack = (char *) malloc(stacksize);
   if (ProcTable[proc_slot].stack == NULL) {
      console("fork1(): Stack allocation failed, returning -1\n");
      return -1;  // make sure to handle allocation failure
   }

   // Sets stack size and initial process status
   ProcTable[proc_slot].stacksize = stacksize;
   ProcTable[proc_slot].status = STATUS_READY;  // Set the process status to READY


   /* CHILDREN */
   // Allocates memory for the child list
   memset(&ProcTable[proc_slot].children, 0, sizeof(ProcList)); // Initialize the children list with 0
   ProcTable[proc_slot].children.pHead = NULL;
   ProcTable[proc_slot].children.pTail = NULL;
   ProcTable[proc_slot].children.count = 0;

   // If the new process has a parent, add it to the parent's child list
   if (ProcTable[proc_slot].pParent != NULL) 
   {
      // Add the new process to the parent's child list
      AddToList(&(ProcTable[proc_slot].pParent->children), &ProcTable[proc_slot]);
   }
   
   // Initialize context for this process with the 'launch' function as the entry point
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);       // MUST be done, before you can switch process contexts

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   // Add process to ready list (ready list priorities are 0-5 so priority 1 goes in ReadyList[0], priority 6 goes in ReadyList[5])
   AddToList(&ReadyList[priority-1], &ProcTable[proc_slot]);

   // Increment number of currently active processes (note: process must utilize quit() function for numProc to be decremented)
   ++numProc;

   // dispatcher
   /* call dispatcher dispatcher(); which will transition the processing
    * to whichever process needs to run next based on the scheduling algorithm 
    */
   if (readyToStart)
   {
      dispatcher(); 
   }

   //return the PID of newly created process
   return ProcTable[proc_slot].pid;
} /* fork1 */

/* TODO: make function*/
/* The interrupts can then be enabled by setting the current interrupt enable 
bit in the processor status register (see Section 3.4). */
static void enableInterrupts()  {
    unsigned int currentPsr = psr_get();
    psr_set(currentPsr | PSR_CURRENT_INT);

}


/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   DebugConsole("launch(): started\n");

   /* Enable interrupts */ // TODO: set error handling
   enableInterrupts();

   // On first launch(), Current is empty
   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   DebugConsole("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id and status of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
   // TODO: return -1 if the parent process was zapped while waiting for a child to quit
int join(int *status) 
{
   int childPid = -1;

   // Check if we're in kernel mode
   checkKernelMode();

   // Check if the process has been zapped while waiting for a child to quit
   if (is_zapped())
   {
      return -1;
   }

   // Check if the process has any children
   if (Current->children.count == 0)
   {
      // if no children, return -2
      // console("Error: join() called with no children - join()\n");
      return -2;
   }

   // Iterate through the child list of the parent process
   proc_ptr child = Current->children.pHead;
   while (child != NULL)
   {
      // Check if the child is in the quitting state
      if (child->status == STATUS_QUIT)
      {
         // Obtain the exit code from the child
         if (status != NULL)
         {
            *status = child->exitCode;
         }

         // Remove children from lists (quit should have removed it from ready list)
         RemoveFromList(&(Current->children), child); // Remove child from parent's children list
         

         // Grab the child's pid before cleaining PCB
         childPid = child->pid;

         // Clean the child's PCB
         //pcbClean(child);

         // Return pid of the terminated child
         return childPid;
      }

      // Move to the next child in the list
      child = child->pNextSibling;
   }

    // No child has quit before join is called, block the parent process
    Current->status = STATUS_BLOCKED_JOIN;
    dispatcher(); // Switch to another process

   // After being unblocked, check if we've been zapped and check again for any quitting child processes
   if (is_zapped())
   {
      return -1;  // we've been zapped while blocked
   }
   return join(status); // check again for any quitting child processes
  
} /* join */ 


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.

   USES pcbClean - WHICH WIPES THE PCB ENTIRELY (EXIT STATUS IS REDUNDANT)
   might change this later

   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{  
   // Check if we're in kernel mode
   checkKernelMode();

   // Clean up the PCB (a.k.a. proc_struct or process table entry) (not entirely because its paremtn may want to join it later)
      // Two cases:
         // 1. Parent has already done a join - join returns what?
         // 2. Parent has not done a join yet
      // Unblock processes that zapped this process
      // May have children who have quit() and completely clean up the PCBs for these zombie children

   // CHILDREN - Check if process has quitting children
   proc_ptr child = Current->children.pHead;
   while (child != NULL)
   {
      if (child->status == STATUS_QUIT)
      {
         // Anakin Skywalker
         // Remove child from ready list
         RemoveFromList(&ReadyList[child->priority-1], child);   // remove it from process list
         // Clear child from the ProcTable (could make this a standalone function)
         for (int i = 1; i < MAXPROC; i++)
         {
            if (ProcTable[i].pid == child->pid)
            {
               // Clear the process entry in the ProcTable
               memset(&ProcTable[i], 0, sizeof(proc_struct));
            }
         }
         // Clean child's PCB ? (or did memset take care of that?)
         //pcbClean(child);
      }
      // Move to next child
      child = child->pNextSibling;
   }

   // ZAP - Check if Current process has been zapped
   if (Current->zapped)
   {  
      // Iterate through Current's list of zappers (processes who've zapped Current)
      proc_ptr zapper = Current->zappers.pHead;
      while (zapper != NULL)
      {
         if (Current->zapped)
         {
            // Wake up the zappers
            if (zapper->status == STATUS_BLOCKED_ZAP)
            {
               zapper->status == STATUS_READY;                          // Set zapper status to ready
               AddToList(&ReadyList[zapper->priority-1], zapper);       // Add zapper to ready list
               RemoveFromList(&Current->zappers, zapper);               // Remove zapper from Current's zapper list
            }
         }
         zapper = zapper->pNextSibling;   // Move to next zapper
      }
   }

   p1_quit(Current->pid);

   // Check if process has already quit
   if (Current->status == STATUS_EMPTY)   // Check if process has already quit
   {  
      // Should never see this code if PCB cleanup is working properly
      console("Warning: Quitting process has already quit, calling dispatcher p quit()\n");
      dispatcher();  // If so, run dispatcher
   } 
   
   Current->status = STATUS_QUIT;   // Marked for death (quit) 
   Current->exitCode = code;        // save exit code 
   
   // If process has a parent
   if (Current->pParent != NULL)
   {
      // Tell the parent there is a quitting child (Done by setting status to STATUS_QUIT, as join looks for this)

      // If current proceess's parent is blocked -- SENTINEL's status is ready so it doesn't trigger the following logic
      if (Current->pParent->status == STATUS_BLOCKED_JOIN)
      {
         // make parent ready to run again
         Current->pParent->status = STATUS_READY;             // Set parent status to ready
         AddToList(&ReadyList[Current->pParent->priority-1], Current->pParent);      // Add parent to ready list
      }

   }
   else
   {
      Current->status = STATUS_EMPTY;
      
      // Remove process from ready list 
      if (Current->pid != 2)
      {
         RemoveFromList(&ReadyList[Current->priority-1], Current);   // remove it from ready list // THIS DIDN't work with start1
      }
      // Remove process from proc list
       for (int i = 1; i < MAXPROC; i++)
         {
            if (ProcTable[i].pid == Current->pid)
            {
               // Clear the process entry in the ProcTable
               memset(&ProcTable[i], 0, sizeof(proc_struct));
            }
         }
         // Clean current's PCB ? (or did memset take care of that?)
         //pcbClean(current);
   }

   // Don't decrement if we're sentinel
   if (Current->pid != 2)
   {
      --numProc;     // Decrement number of active processes 
   }

   dispatcher();
} /* quit */

/* ------------------------------------------------------------------------
   Name - zap
   Purpose - zaps a process, marking it for termination
   Parameters - process's PID
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
int zap(int pid)
{
   int result = 0;
   proc_ptr pProcToZap;
   checkKernelMode();   // Check we're in kernel mode

   // Check if the process is trying to zap itself
   if (pid == Current->pid) {
      console("Error: Process cannot zap itself\n");
      return -1;
   }

   // Find the process to be zapped
   proc_ptr pProcToZap = &ProcTable[pid % MAXPROC];
   
   // Check if the process exists
   if (pProcToZap->status == STATUS_EMPTY) {
      console("Error: Process does not exist\n");
      halt(1);
   }

   // Mark the process as zapped
   pProcToZap->zapped = 1;
   AddToList(&pProcToZap->zappers, Current); // Add current process to the zapped process's list of zappers

   // block until process calls quit()
   Current->status == STATUS_BLOCKED_ZAP;
   dispatcher();
}


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void) {
    proc_ptr next_process;
    context *pPrevContext=NULL;
    int currentClock = sys_clock();

    // TODO: Check if current process can continue running - DONE - implemented in GetNextReadyProc
    // Has process been time-sliced?
    // Has process been blocked?
    // Is process still the highest priority among READY processes?
      // If not, it has to go back on the ready list

   // Handle case where child is higher priority than parent that still needs to run
   // Check if the parent process of the next process is still running

    // Find the next process to run (returns current process or new process)
    next_process = GetNextReadyProc();

    // Is the process changing
    if (next_process != Current)
    {
      // If there is a currently running process
      if (Current != NULL)
      {  
         pPrevContext = &Current->state; // Populate pPrevContext with current state
      }
         Current = next_process; // Assign Current to new process
         Current->status = STATUS_RUNNING; // Change the current status to running
         Current->tsStart = currentClock;  // Save start time to tsStart (time slice start)
         // Set old status to blocked?
         context_switch(pPrevContext, &Current->state); // switch contexts to new process
    }
      // NOTE: First context switch from NULL handled in fork1
} /* dispatcher *

/* ------------------------------------------------------------------------
   Name - PopList()
   Purpose - to pop a process off the end of a linked list 
   Parameters - the ProcList to pop an item off of

   Do we want any additional argument passed?

   You can use this function to add processes to the ready list like this:
   PopList(&ReadyList[process_priority]);
   ------------------------------------------------------------------------ */
proc_ptr PopList(ProcList *list)
{  
   // Check if list is empty
   if (list->count == 0)
   {
      return NULL;
   }
   
   // Get the oldest item and replace list's head
   proc_ptr poppedProc = list->pHead;  // get the head of the list (oldest item)
   list->pHead = poppedProc->pNextSibling; // update the head to the next process

   // Update head/tail pointers
   if (list->pHead == NULL)
   {
      list->pTail = NULL;  // If the head becomes NULL (no more items), update the tail as well
   }
   else
   {
      list->pHead->pPrevSibling = NULL; // Update the new head's previous pointer to NULL
   }

   // Update the popped process's pointers
   if (poppedProc->pNextSibling != NULL)
   {
      poppedProc->pNextSibling->pPrevSibling = NULL; // Update the next process's previous pointer to NULL
   }


   // Decrement the count of processes in the list
   list->count--;

   return poppedProc;
}
/* ------------------------------------------------------------------------
   Name - AddToList
   Purpose - Add a process to a process list
   Parameters - The ProcList to add an item to, the process to add)

   You can use this function to add processes to the ready list like this:
   AddToList(&ReadyList[process_priority], new_process); 
   ------------------------------------------------------------------------ */
int AddToList(ProcList *list, proc_ptr process)
{
   if (process == NULL)
   {
      // Invalid process pointer
      return 0;
   }

   // Check if process is already on the ReadyList
   proc_ptr current = list->pHead;
   if (current != NULL)
   {
      if (current->pid == process->pid)
      {
         // Process is already in the list
         return 0;
      }
      // would update current to sibling, but if the new proc is a sibling it won't be added
    }

   // Update the new process's pointers 
   process->pNextSibling = NULL;         // The new process will be the last one, so its next pointer is NULL
   process->pPrevSibling = list->pTail;  // Its previous pointer points to the current tail

   if (list->pTail != NULL)
   {
      // Update the current tail's next pointer to the new process if list is not empty and if it is not already updated
      if (list->pTail->pNextSibling != process)
      {
         list->pTail->pNextSibling = process;
      }
   }

   // The list's new tail is the new process
   list->pTail = process;

   if (list->pHead == NULL)
   {
      // If the list was empty, update the head to point to the new process
      list->pHead = process;
   }

   // Increment the count of processes in the list
   list->count++;

   // Return 1 for success
   return 1;
}

/* ------------------------------------------------------------------------
   Name - GetNextPid
   Purpose - Get next available PID
   ------------------------------------------------------------------------ */
int GetNextPid()
{
   int newPid = -1;                       // Initialize new pid to -1
   int procSlot = next_pid % MAXPROC;     // Assign new process to next_pid mod MAXPROC (to wrap around to 1, 2, 3, etc. from 50)

   if (numProc < MAXPROC)
   {
      while (numProc < MAXPROC && ProcTable[procSlot].status != STATUS_EMPTY)
      {
         next_pid++;
         procSlot = next_pid % MAXPROC;
      }
      newPid = next_pid++;                // Assigns newPid to current next_pid value, then increments next_pid
   }

   return newPid;
}

/* ------------------------------------------------------------------------
   Name - GetNextReadyProc
   Purpose - Get the next ready process from the ready list
   ------------------------------------------------------------------------ */
proc_ptr GetNextReadyProc()
{
   proc_ptr nextProc = NULL;
   int lookForNewProcess = 0;

   // first see if the current process is the highest

   // if there are processes running,
   if (Current != NULL && Current->status == STATUS_RUNNING)
   {  
      nextProc = Current;

      
      // Go through the ready list and see if any process is higher priority than Current (if highest priority, won't check)
      for (int i = 0; i <= Current->priority-1; ++i)
      {
         if (ReadyList[i].count > 0) // If there is and it is not it's child,
         {
            // If there is, ensure it's not the child of current
            if (ReadyList[i].pHead->pParent == Current)
            {
               return Current;
            }
            // look for new process
            lookForNewProcess = 1;
         }
      }
   }
   // If process is quitting
   //else if (Current != NULL && Current->status == STATUS_QUIT)
   //{
      // Remove it from ready list

      // return its parent
     // return Current->pParent;
   //}
   else
   {
      lookForNewProcess = 1;
   }

   if (lookForNewProcess)
   {  
      // pull next entry from the ReadyList
      for (int i = 1; i <= SENTINELPRIORITY; ++i)
      {
         // if there is an entry for this priority, get the first one (goes from 1 to 6)
         if (ReadyList[i-1].count > 0)
         {
            // If this is start1
            if (ReadyList[i-1].pHead->pid == 2)
            {
               nextProc = PopList(&ReadyList[i-1]);
               return nextProc;
            }
            // If the next ready process is not the child of the current process
            else if (ReadyList[i-1].pHead->pParent != Current)
            {
               nextProc = PopList(&ReadyList[i-1]);
               return nextProc;
            }
            // If the next ready item is the child of the current process and the current process status is not running
            else if (ReadyList[i-1].pHead->pParent == Current && ReadyList[i-1].pHead->pParent->status != STATUS_RUNNING)
            {
               // get the next ready item of the Ready List (pop it off the list and return)
               nextProc = PopList(&ReadyList[i-1]);
               return nextProc;
            }
         }
      }      
   }
   return nextProc; 
}

/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
// changed from void to char * again to meet phase1.h signature/usloss infrastructure
int sentinel (char * dummy)
{
   DebugConsole("sentinel():called\n");
   while (1) // forever loop
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* ------------------------------------------------------------------------
   Name - check_deadlock
   Purpose - check to determine if deadlock has occurred... 
   ------------------------------------------------------------------------ */
static void check_deadlock()
{
   if (check_io() == 1) // In a future phase- dummy that always returns 0. You need to provide its definition.
      return;
   
   /* Has everyone terminated? */
   // Check number of processes
   if (numProc == 1)
   {
      // If there is only one active process, me,
      printf("All processes completed.\n"); 
      // halt(0)
      halt(0);
   }
   else
   {
      // otherwise halt(1)
      printf("DEADLOCK DETECTED.\n");
      halt(1);
   }
} /* check_deadlock */

// Dummy function for future phase
int check_io()
{
   return 0;
}

/* ------------------------------------------------------------------------
   Name - disableInterrupts
   Purpose - Disables the interrupts.
   ------------------------------------------------------------------------ */
void disableInterrupts()
{
   /* turn the interrupts OFF iff we are in kernel mode */
   checkKernelMode();
  
   /* We ARE in kernel mode */
   psr_set( psr_get() & ~PSR_CURRENT_INT );

} /* disableInterrupts */


/* TODO: Clock Handler function */
void clockHandler(int dev, void *pUnit)
{
   // Clock interrupt has occurred
   // time slice (check if time is up, if so, make ready and dispatch)
   time_slice();
}

// check if time is up, if so, make ready and dispatch
void time_slice(void)
{
   // get system time
   int currentTime = sys_clock();   // sys_clock() returns time in microseconds

   // subtract current time from process time 
   if (currentTime - Current->tsStart > 80000) // 80000 microseconds is 80 milliseconds
   {
      // if greater than 80ms, time's up!
      // Set process status to READY
      Current->status = STATUS_READY;
      // Add process back to ready list
      AddToList(&ReadyList[Current->priority-1], Current);
      // Call dispatcher
      dispatcher();
   }
}

/* ---------------------------------------------------------
   Name - DebugConsole
   Purpose - Prints the message to the console if in debug mode
   Parameters - format string and va args
   Returns - nothing
   Side Effects -
   ---------------------------------------------------------*/
void DebugConsole(char *format, ...)
{
   if (DEBUG && debugflag)
   {
      va_list argptr;
      va_start(argptr, format);
      console(format, argptr);
      va_end(argptr);
   }
}

/* ---------------------------------------------------------
   Name - checkKernelMode
   Purpose - Halts if we are not in kernel mode, else return 1
   Parameters - none
   Returns - 1 if we are in kernel mode
   Side Effects -
   ---------------------------------------------------------*/
void checkKernelMode()
{
   if((PSR_CURRENT_MODE & psr_get()) == 0) 
   {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
   }
}

/* ---------------------------------------------------------
   Name - pcbClean
   Purpose - Cleans up the pcb of the process
   Parameters - none
   Returns - none
   Side Effects -
   ---------------------------------------------------------*/
void pcbClean(proc_ptr pcb)
{
   if (pcb != NULL)
   {
      free(pcb);  // Free the pcb stack
      memset(pcb, 0, sizeof(struct proc_struct)); // Clear the content of the PCB structure
      free(pcb);  // free the memory allocated for the PCB
   }
}

int RemoveFromList(ProcList* list, proc_ptr process) 
{
    if (process == NULL) 
    {
        // Invalid process pointer
        return 0;
    }

    // Check if the process to remove is the head of the list
    if (list->pHead == process) 
    {
        // Update the head to point to the next process
        list->pHead = process->pNextSibling;
    }

    // Check if the process to remove is the tail of the list
    if (list->pTail == process) 
    {
        // Update the tail to point to the previous process
        list->pTail = process->pPrevSibling;
    }

    // Update the sibling pointers of adjacent processes
    if (process->pNextSibling != NULL) 
    {
        process->pNextSibling->pPrevSibling = process->pPrevSibling;
    }
    if (process->pPrevSibling != NULL) 
    {
        process->pPrevSibling->pNextSibling = process->pNextSibling;
    }

    // Reset the removed process's pointers
    process->pNextSibling = NULL;
    process->pPrevSibling = NULL;

    // Decrement the count of processes in the list
    list->count--;

    // Return 1 for success
    return 1;
}
/* ---------------------------------------------------------
   Name - dump_processes
   Purpose -  For each PCB in the process table print (at a minimum) its PID, parent’s 
              PID, priority, process status (e.g. unused, running, ready, blocked, etc.), 
              # of children, CPU time consumed, and name
   Parameters - none
   Side Effects -
---------------------------------------------------------*/
void dump_processes(void)
{
   proc_ptr process;

   // Collumn headers
   printf("%-5s%-8s%-10s%-15s%-8s%-10s%s\n", "PID", "Parent", "Priority", "Status", "# Kids", "CPUtime", "Name");


   // Traverse through each process
   for (int i = 0; i < MAXPROC; ++i)
   {
      process = ProcTable[i];

      // Print process information
      printf("%-5d%-8d%-10d%-15s%-8d%-10d%s\n", 
            process->pid, 
            process->pParent ? process->pParent->pid : -1, 
            process->priority, 
            process->status == STATUS_EMPTY ? "EMPTY" :
            process->status == STATUS_READY ? "READY" :
            process->status == STATUS_RUNNING ? "RUNNING" :
            process->status == STATUS_BLOCKED_JOIN ? "BLOCKED" :
            "UNKNOWN",
            process->children.count,
            process->cpu_time,
            process->name); 
   }
}

int is_zapped(void)
{
   checkKernelMode();   // Ensure we are in kernel mode

   // Return 1 if Current status is STATUS_ZAPPED, otherwise return 0
   if (Current->zapped)
   {
      return 1;
   }
   else return 0;
}

// Returns pid from current process
int getpid(void)
{
   int pid = Current->pid; // Grab pid from current process
   return pid; // Return it
}

// Blocks current process (new status must be larger than 10)
int block_me(int new_status)
{
   int result = 0;

   // validate the parameter
   if (new_status <= 10)
   {
      console("Error: block_me called with new_status less than or equal to 10 block_me()\n");
      halt(1);
   }

   // set new status and call dispatcher
   Current->status = new_status;
   dispatcher();

   // have we been zapped while blocked?
   if (is_zapped())
   {
      return -1;  
   }

   return result;
}/* block me */

int unblock_proc(int pid)
{
   int result = -1;
   int procSlot;
   proc_ptr pProcToUnblock;

   // validate pid
   if (pid == -1 || pid > MAXPROC)
   {
      console("Error: unblock_proc() called with invalid pid: %d", pid);
      halt(1);
   }

   // Check if calling process is zapped
   if (is_zapped)
   {
      return -1;
   }

   procSlot = pid % MAXPROC;
   pProcToUnblock = &ProcTable[procSlot];

   // check its current status (ensure it's not 10 or below)
   if (pProcToUnblock->status <= 10)
   {
      console("Error: unblock_proc() called with status less than or equal to 10\n");
      halt(1);
   }

   // Set status to ready and Add to ready list
   pProcToUnblock->status = STATUS_READY; // Set ready status
   AddToList(&ReadyList[pProcToUnblock->priority-1], pProcToUnblock); // Add to ready list
   dispatcher();  // anytime you make a change to ready list, dispatch

   return result;
}