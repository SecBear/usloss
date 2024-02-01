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


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

// FIRST TODO: EXAMINE THIS - the master list of all the processes that are active - any process could be ready, blocking, running
/* the process table */
proc_struct ProcTable[MAXPROC];  // Just an array that can hold 50 processes

/* Process lists  */

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

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
      ProcTable[i].status = UNUSED;   //marking each entry as unused
      ProcTable[i].pid = UNUSED;      //Using same constant to mark PID as unused
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
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
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

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   if ((psr_get() & PSR_CURRENT_MODE) == 0) {
      console("fork1(): not in kernel mode, halting...\n");
      halt(1);
   }
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

   if (f == NULL || name == NULL) {
      console("fork1(): function or name is NULL, returning -1\n");
      return -1;
   }
   /* find an empty slot in the process table */
   proc_slot = -1;
   for (int i = 1; i < MAXPROC; i++) {
        if (ProcTable[i].status == UNUSED) {
         proc_slot = i;
         break;
        }
   }

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
   if ( arg == NULL )                        // Check if arguments need to be passed
      ProcTable[proc_slot].start_arg[0] = '\0'; // If none, set the process's start_arg element to NULL
   else if ( strlen(arg) >= (MAXARG - 1) ) {    // Checks to see if it's argument is too long
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else                                      // Put the argument address (arg) into the process entry's start_arg element
      strcpy(ProcTable[proc_slot].start_arg, arg);

   /* Assign PID to process */
   ProcTable[proc_slot].pid = proc_slot; // Assign the process PID to proc_slot

   // Allocates stack space for the new process
   ProcTable[proc_slot].stack = (char *) malloc(stacksize);
   if (ProcTable[proc_slot].stack == NULL) {
      console("fork1(): Stack allocation failed, returning -1\n");
      return -1;  // make sure to handle allocation failure
   }

   // Sets stack size and initial process status
   ProcTable[proc_slot].stacksize = stacksize;
   ProcTable[proc_slot].status = READY;  // Set the process status to READY
   
   // Initialize context for this process with the 'launch' function as the entry point
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);       // MUST be done, before you can switch process contexts

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   // Assign Process Priority
   ProcTable[proc_slot].priority = priority; 

   // TODO: Add process to ready list (ready list priorities are 0-5 so priority 1 goes in ReadyList[0], priority 6 goes in ReadyList[5])
   AddToList(&ReadyList[priority-1], &ProcTable[proc_slot]);

   // TODO: dispatcher
   /* call dispatcher dispatcher(); which will transition the processing
    * to whichever process needs to run next based on the scheduling algorithm 
    */
   if (readyToStart)
   {
      dispatcher(); 
   }

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

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   p1_quit(Current->pid);
} /* quit */


/* 
   Name - PopList()
   Purpose - to pop a process off the end of a linked list 
   Parameters - the ProcList to pop an item off of

   Do we want any additional argument passed?

   You can use this function to add processes to the ready list like this:
   PopList(&ReadyList[process_priority]);
*/
proc_ptr PopList(ProcList *list)
{  
   // Check if list is empty
   if (list->count == 0)
   {
      return NULL;
   }
   
   // Get the oldest item and replace list's head
   proc_ptr poppedProc = list->pHead;  // get the head of the list (oldest item)
   list->pHead = poppedProc->next_proc_ptr; // update the head to the next process

   // Update head/tail pointers
   if (list->pHead == NULL)
   {
      list->pTail = NULL;  // If the head becomes NULL (no more items), update the tail as well
   }
   else
   {
      list->pHead->prev_proc_ptr = NULL; // Update the new head's previous pointer to NULL
   }

   // Decrement the count of processes in the list
   list->count--;

   return poppedProc;
}
/*
   Name - AddToList
   Purpose - Add a process to a process list
   Parameters - The ProcList to add an item to, the process to add)

   You can use this function to add processes to the ready list like this:
   AddToList(&ReadyList[process_priority], new_process);
*/
int AddToList(ProcList *list, proc_ptr process)
{
   if (process == NULL)
   {
      // Invalid process pointer
      return 0;
   }

   // Update the new process's pointers 
   process->next_proc_ptr = NULL;         // The new process will be the last one, so its next pointer is NULL
   process->prev_proc_ptr = list->pTail;  // Its previous pointer points to the current tail

   if (list->pTail != NULL)
   {
      // Update the current tail's next pointer to the new process if list is not empty
      list->pTail->next_proc_ptr = process;
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


// TODO: Get the next ready process from the ready list
proc_ptr GetNextReadyProc()
{
   // pull next entry from the ReadyList
   for (int i = 1; i <= SENTINELPRIORITY; ++i)
   {
      // if there is an entry for this priority, get the first one (goes from 1 to 6)
      if (ReadyList[i-1].count > 0)
      {
         // get the next ready item of the Ready List
         return PopList(&ReadyList[i-1]);
      }
   }

   // Error: code should not reach this block
   printf("Error: GetNextReadyProc()\n");
}

// TODO: 
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

    // Find the next process to run
    next_process = GetNextReadyProc();

    if (next_process != NULL) {
        // Perform the context switch
        if (Current != NULL) {
            context_switch(&Current->state, &next_process->state);
        } else {
            context_switch(NULL, &next_process->state);
        }

        // Update the current process
        Current = next_process;
    } else {
        // Handle the case where there are no ready processes
        console("dispatcher: No ready processes.\n");
    }
}

/* dispatcher */


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
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1) // forever loop
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
} /* check_deadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */
