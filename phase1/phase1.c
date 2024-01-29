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
int sentinel (void *);
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
   int * ReadyList = NULL; // Note: not sure what ReadyList is supposed to be, made it int * for now

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
   for (int i = 0; i < MAXPROC; i++) {
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
   ProcTable[proc_slot].start_func = f;      // Start the function for the process, assign output to f
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
ProcTable[proc_slot].status = READY;  // Set the process status to READY
   
// Initialize context for this process with the 'launch' function as the entry point
context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);       // MUST be done, before you can switch process contexts

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   // TODO: dispatcher
   /* call dispatcher dispatcher(); which will transition the processing
    * to whichever process needs to run next based on the scheduling algorithm 
    */
   dispatcher(); // "when to call dispatcher? there's one nuance here but I want you to think about it before I give you the answer"

} /* fork1 */

/* TODO: make function*/
static void enableInterrupts()
{


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
void dispatcher(void)
{
   proc_ptr next_process;

   p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


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
// Changed from char * to void * (to match above)
int sentinel (void * dummy)
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
