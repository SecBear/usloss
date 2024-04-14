#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <sems.h>
#include <stdio.h>
#include <stdlib.h>
#include <provided_prototypes.h>
#include <usyscall.h>
#include <libuser.h>
#include <time.h>

int start2(char *); 
int start3(char *);
void syscall_handler(int dev, void *punit);
static int spawn_launch(char *arg);
static void nullsys3(sysargs *args_ptr);
void check_kernel_mode(char string[]);
int launchUserMode(char *arg);
static void syscall_spawn(sysargs *args);
int syscall_wait(sysargs *args);
void syscall_terminate(sysargs *args);
void syscall_semcreate(sysargs *args);
int GetNextSemID();
void syscall_semp(sysargs *args);
void syscall_semv(sysargs *args);
int syscall_getpid(sysargs *args);
void syscall_semfree(sysargs *args);
int syscall_gettimeofday(sysargs *args);
void syscall_getcputime(sysargs *args);

/* ------------------------------------------------------------------------
   Global Variables
   ----------------------------------------------------------------------- */
process ProcTable[MAXPROC];     // Array of processes
semaphore SemTable[MAXSEMS];       // Array of seamphores

int numSems = 0;                // Global count of semaphores
int next_sem_id = 0;            // Integer to hold the next semaphore ID
int numWaitingProc = 0;         // Integer to hold the number of waiting processes
/* ------------------------------------------------------------------------ */


/* ------------------------------------------------------------------------
   Functions

/* ------------------------------------------------------------------------
   Name - Start2
   Purpose - Create first user-level process and wait for it to finish.
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
start2(char *arg)
{
    int		pid;
    int		status;

    // Check that we're in kernel mode
    check_kernel_mode("Check_Kernel_Mode: Start2\n");

    for (int i = 0; i < MAXSYSCALLS; i++)
    {
        // Initialize every system call handler as nullsys3;
        sys_vec[i] = nullsys3;
    }

    // Initialize each system call handler that is required individually
    sys_vec[SYS_SPAWN] = syscall_spawn;         // spawn 
    sys_vec[SYS_WAIT] = syscall_wait;           // wait
    sys_vec[SYS_TERMINATE] = syscall_terminate; // terminate
    sys_vec[SYS_SEMCREATE] = syscall_semcreate; // semcreate
    sys_vec[SYS_SEMP] = syscall_semp;           // semp
    sys_vec[SYS_SEMV] = syscall_semv;           // semv
    sys_vec[SYS_SEMFREE] = syscall_semfree;     // semfree
    sys_vec[SYS_GETTIMEOFDAY] = syscall_gettimeofday; // get time of day
    sys_vec[SYS_CPUTIME] = syscall_getcputime;  // cpu time
    sys_vec[SYS_GETPID] = syscall_getpid;       // get pid
    int_vec[SYSCALL_INT] = syscall_handler;     // system call handler

    // Initialize Process Table
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

    // Initialize Semaphore Table
    for (int i = 0; i < MAXSEMS; i++)
    {
        // Initialize basic values
        SemTable[i].value = NULL;
        SemTable[i].mbox = NULL;
        SemTable[i].sid = NULL;
        SemTable[i].status = SEM_UNUSED;  // indicates a semaphore is freshly allocated

        // Initialize waiting list
        SemTable[i].waiting = malloc(sizeof(struct list));
        SemTable[i].waiting->pHead = NULL;  
        SemTable[i].waiting->pTail = NULL;  
        SemTable[i].waiting->count = 0;     
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscall_handler; spawn_real is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes usyscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawn_real().
     *
     * Here, we only call spawn_real(), since we are already in kernel mode.
     *
     * spawn_real() will create the process by using a call to fork1 to
     * create a process executing the code in spawn_launch().  spawn_real()
     * and spawn_launch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawn_real() will
     * return to the original caller of Spawn, while spawn_launch() will
     * begin executing the function passed to Spawn. spawn_launch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawn_real() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawn_real("start3", start3, NULL, 4*USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

} /* start2 */

/* ------------------------------------------------------------------------
   Name - syscall_spawn
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
static void syscall_spawn(sysargs *args)
{
    int(*func)(char *);
    char *arg;
    int stack_size;
    int priority;
    char *name;
    // more local variables
    if (is_zapped)
    {
        //Terminate(1);   // terminate the process
    }

    func = args->arg1;
    arg = args->arg2;
    stack_size = (int) args->arg3;
    priority = args->arg4; 
    name = (char *)args->arg5;

    // call another function to modularize the code better
    int kid_pid = spawn_real(name, func, arg, stack_size, priority);    // spawn the process
    args->arg1 = (void *) kid_pid;  // packing to return back to caller
    args->arg4 = (void *) 0;

    if (is_zapped()) // should terminate the process
    {
        // Set to user mode - call psr_set to do this
        return ;
    }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int  spawn_real(char *name, int (*func)(char *), char *arg,
                int stack_size, int priority)
{
    // mbox create to create a private mailbox
    // call fork1 to create a process that runs a start function
    // the process runs at user mode
    // maintain the parent-child relationship at phase 3 process table
    // provide a launch function: spawn_launch()

    int pid;
    int my_location; /* parent's location in process table */
    int kid_location; /* child's location in process table */
    int result;
    int startupMbox;

    process *kidptr, *prevptr;
    my_location = getpid() % MAXPROC;

    /* create our child */
    pid = fork1(name, launchUserMode, arg, stack_size, priority);
    //                   |-> change to launchUserProcess which waits for process to be initialized with parent, proclist, etc. before running (in the case of higher priority child)
    if (pid >= 0)
    {
        int procSlot = pid % MAXPROC;
        ProcTable[procSlot].pid = pid;
        ProcTable[procSlot].parentPid = getpid();
        ProcTable[procSlot].entryPoint = func;          // give launchUserMode the function call 
        ProcTable[procSlot].name = name;
        ProcTable[procSlot].priority = priority;

        // Add process to parent's children list
        AddList(pid, ProcTable[my_location].children);

        ProcTable[procSlot].status = STATUS_RUNNING;    // Set process status to running
        MboxCondSend(ProcTable[procSlot].startupMbox, NULL, 0);  // Tell process to start running (unblock in launchUserMode)
    }
    return pid;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int launchUserMode(char *arg)
{   
    int pid;
    int procSlot;
    int result;
    int psr;

    pid = getpid();
    procSlot = pid % MAXPROC;

    // If this process pre-empts the procTable initialization, wait until that's done
    MboxReceive(ProcTable[procSlot].startupMbox, NULL, 0);  // Blocks until a message is in the startup mbox

    // set user mode using get_psr and set_psr
    psr = psr_get();
    psr = psr & ~PSR_CURRENT_MODE;   // Unset the current mode bit (to user mode)
    psr_set(psr);

    // run the entry point
    result = ProcTable[procSlot].entryPoint(arg);

    // After process returns
    Terminate(result);
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int syscall_wait(sysargs *args)
{
    int *status = (void *)args->arg2;
    int result = wait_real(status);
    if (result >= 0)
    {
        args->arg1 = (void *)result;
        args->arg2 = (void *)ProcTable[result % MAXPROC].termCode;
        // check for children
        if (ProcTable[result % MAXPROC].children->count > 0)
        {
            args->arg4 = (void *)0;     // process has children
        }
    }
    else if (result == -2)
    {
        // process has no children
    }
    else if (result == -1)
    {
        // process was zapped while waiting for child to quit
    }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
extern int wait_real(int *status)
{
    int result = join(&status);
    return result;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_terminate(sysargs *args)
{
    int exit_code = (void *) args->arg1;
    terminate_real(exit_code);
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
extern void terminate_real(int exit_code)
{
    /* Terminates the invoking process and all its children and synchronizes with its parent’s Wait
    system call. The child Processes are terminated by zap’ing them. When all user processes have
    terminated, your operating system should shut down. Thus, after start3 terminates (or
    returns) all user processes should have terminated. Since there should then be no runnable or
    blocked processes, the kernel will halt.
    */
   int pid = getpid();
   process *current = &ProcTable[pid % MAXPROC];

   current->status = STATUS_TERMINATED; // Set status to terminated
   if (current->termCode != 1)  // check if termination code is already set by semfree
    {
        current->termCode = exit_code;
    }

    // if process has children
    if (current->children->count > 0)
    {
        // Zap each child
        process *current_child = current->children->pHead; 
        while (current_child != NULL)
        {
            // check if child has already terminated
            if (current_child->status == STATUS_TERMINATED)
            {
                popList(current->children);
                current_child = current_child->pNext;
                break;
            }
            // If not, zap the child and wake it up
            zap(current_child->pid);
            MboxCondReceive(current_child->privateMbox, NULL, 0);
            popList(current->children); // Child should be done now, so pop it
            current_child = current_child->pNext;
        }
    }

   // Quit (terminate) this process
    quit(exit_code);
}


/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_semcreate(sysargs *args) {   //useless function having issues with result
    int init_value = (int)(long)args->arg1;

    int sem_id = semcreate_real(init_value);
    if (sem_id >= 0) {
        // Success: Return semaphore ID and set result to 0
        args->arg1 = (void *)(long)sem_id; // Correctly returning semaphore ID
        args->arg4 = (void *)0;            // Indicating success
    } else {
        // Failure: Indicate failure in creating a semaphore
        args->arg1 = (void *)(long)-1;     // Semaphore not created, so returning -1
        args->arg4 = (void *)(long)-1;     // Indicate failure
    }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int semcreate_real(int init_value) {     //this is dumb and needs fixing

    // Get the next semaphore ID
    int semID = GetNextSemID();

    // Check validity before allocating semaphore values
    if (semID == -1)    
    {
        return -1;
    }

    // Initialize semaphore values
    SemTable[semID].status = SEM_USED;
    SemTable[semID].value = init_value;
    SemTable[semID].mbox = MboxCreate(0,0);     // Create semaphore's private mailbox
    SemTable[semID].mutex = MboxCreate(1,0);    // Create semaphore's mutex
    
    numSems++;  // Increment max number of sems
    
    return semID;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int GetNextSemID()
{
   int new_sem_id = -1;                  // Initialize new mbox id to -1
   int semSlot = next_sem_id % MAXMBOX; // Assign new mailbox to next_mbox_id mod MAXMBOX (to wrap around to 1, 2, 3, etc. from max)

   if (numSems < MAXSEMS) // If there's room for another process
   {
      // Loop through until we find an empty slot
      while (SemTable[semSlot].status != SEM_UNUSED) //&& semSlot != next_sem_id)
      {
         next_sem_id++;
         next_sem_id = next_sem_id % MAXMBOX;
         semSlot = next_sem_id % MAXSEMS;
      }

      if (SemTable[semSlot].status == SEM_UNUSED)
      {
         new_sem_id = next_sem_id;                  // Assigns new_mbox_id to current next_mbox_id value
         next_sem_id = (next_sem_id + 1) % MAXSEMS; // Increment next_mbox_id for the next search
      }
   }

   return new_sem_id;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_semv(sysargs *args)
{
    int semID = (int)args->arg1;    // parse argument (semaphore ID)
    semv_real(semID);               // call semv_real with semID
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - Increments a semaphore.
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int  semv_real(int semID)
{
    semaphore *sem = &SemTable[semID];
    int pid = getpid();                 // Get the pid of current process
    process *current_proc = &ProcTable[pid]; // Get the current process

    // Increment the value
    MboxSend(sem->mutex, NULL, 0);       // Get the mutex
    sem->value++;
    MboxReceive(sem->mutex, NULL, 0);    // Release the mutex

    // Is there any process blocked on the semaphore because of P operation?
    if (sem->waiting->count > 0)
    {  
        // Conditional send on that process's private mailbox
        process *pNext = sem->waiting->pHead;
        while (pNext != NULL)   // traverse through waitlist to find process
        {   
            popList(sem->waiting);  // Remove process from waiting list
            sem->status = SEM_USED;     // Set the semaphore status to used again
            MboxCondSend(pNext->privateMbox, NULL, 0); // Wake up the blocked proc
            break;
        }
    }

    return 0; // success
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_semp(sysargs *args)
{
    int semID = (int)args->arg1;    // parse argument (semaphore ID)
    semp_real(semID);               // call semp_real with semID
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - Decrements a sempahore
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int  semp_real(int semID)
{
    semaphore *sem = &SemTable[semID];  // Get the semaphore
    int pid = getpid();                 // Get the pid of current process
    process *process = &ProcTable[pid]; // Get the current process

    // if the semaphore value >0
    if (sem->value > 0)
    {
        // we'll decrement 
        MboxSend(sem->mutex, NULL, 0);    //obtain mutex
        sem->value--;   // decrement semaphore value
        MboxReceive(sem->mutex, NULL, 0);    // release mutex
    }
    else
    {
        // Otherwise, add process to waiting list (we're trying to decrement below 0)
        AddList(pid, sem->waiting);                  // Add process to wait list
        MboxReceive(process->privateMbox, NULL, 0); // block by receiving on the current process's mailbox?

        if (sem->status == SEM_FREE) // If we've been free'd
        {   
            process->termCode = 1;        // Set status to 1 - there are processes blocked on the semaphore
            terminate_real(pid);         // Terminate
        }
        /*if (is_zapped)  // If we've been zapped
        {
            terminate_real(pid);
        }*/
        MboxSend(sem->mutex, NULL, 0);    // obtain mutex
        sem->value--;   // Still decrement?
        MboxReceive(sem->mutex, NULL, 0);    // release mutex
    }

   return 0;    // success
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_semfree(sysargs *args)
{
    int semID = args->arg1;             // grab sem ID
    int result = semfree_real(semID);   // call semfree_real
    if (result == -1)
    {
        args->arg4 = (void *)(long)-1;        // Indicating semaphore handle is invalid
    }
    else if (result == 0)
    {
        args->arg4 = (void *)0;            // Indicating success
    }
    else if (result == 1)
    {
        args->arg4 = (void *)1;         // Indicating processes were blocked on semaphore
    }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int semfree_real(int semID)
{
    semaphore *sem = &SemTable[semID];  // Get semaphore
    int pid = getpid();                 // Get the pid of current process
    process *proc = &ProcTable[pid]; // Get the current process
    int result = 0;

    // error checking
    if (sem == NULL)
    {
        return -1;
    }

    // obtain mutex
    MboxSend(sem->mutex, NULL, 0);
    sem->status = SEM_FREE;                     // Set semaphore status to free

    // any processes waiting on the semaphore?
    if (sem->waiting->count > 0)
    {
        // terminate them
        process *current = sem->waiting->pHead;
        while (current != NULL)
        {
            popList(sem->waiting);
            MboxCondSend(current->privateMbox, NULL, 0);   // Wake up the process (should terminate with above status)
            current = current->pNext;
        }
        result = 1;
    }

    sem->status = SEM_UNUSED;   // Set status back to unused 

    // release mutex
    MboxReceive(sem->mutex, NULL, 0);

    --numSems;  // Decrement global semaphore count

    return result;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_handler(int dev, void *punit) 
{
   check_kernel_mode("sys_handler");
   sysargs *args = (sysargs*)punit;

   if (dev != SYSCALL_INT) {
      halt(1); // Only proceed if the interrupt is a syscall interrupt
   }
   // check if invalid sys number
   if (args->number >= MAXSYSCALLS)
   {
      printf("syscall_handler(): sys number %d is wrong. Halting...\n", args->number);
      halt(1);
   }
   else if (args == NULL || args->number < 0) {
      nullsys3(args);
   } else
   {
      sys_vec[args->number](args);
   }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
static void nullsys3(sysargs *args_ptr)
{
    printf("nullsys3(): Invalid syscall %d\n", args_ptr->number);
    printf("nullsys3(): process %d terminating\n", getpid());
    terminate_real(1);
} /* nullsys3 */

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void check_kernel_mode(char string[])
{
   int currentPsr = psr_get();

   // if the kernel mode bit is not set, then halt
   // meaning if not in kernel mode, halt(1)
   if ((currentPsr & PSR_CURRENT_MODE) == 0)
   {
      // not in kernel mode
      console("%s, Kernel mode expected, but function called in user mode.\n", string);
      halt(1);
   }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int syscall_gettimeofday(sysargs *args)
{
    // call sys_clock()
    int result = sys_clock();
    args->arg1 = (void *)result;  // packing to return back to caller
    return result;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
void syscall_getcputime(sysargs *args)
{
   
    int result = readtime();
    args->arg1 = (void *)result;
    return;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int syscall_getpid(sysargs *args)
{
    int pid = getpid();
    args->arg1 = pid;
    return pid;
}

/* ------------------------------------------------------------------------
   Name - AddList
   Purpose - Adds the current process to the specified linked list.
   Parameters - int pid: the PID of the process to add.
                list list: the list pointer to add the process to.
   Returns - 1 if the process is successfully added to the waiting list, 0 otherwise.
   Side Effects - May increase the count of the waiting processes.
   ----------------------------------------------------------------------- */
int AddList(int pid, list list)
{
    process *waiting_process = &ProcTable[pid % MAXPROC];  // Get process

    // Add process to mailbox's waiting list
    if (pid == NULL)
    {
        // Invalid process pointer
        return 0;
    }

    // Check if the process is already on the list
    if (list->count > 0)
    {
        process *current = list->pHead;
        if (current != NULL)
        {
            if (current->pid == pid)
            {
                // Process is already in the list
                return 0;
            }
            current = current->pNext; // Move to next process
        }
    }

    // Update new waiting process's pointers
    waiting_process->pNext = NULL;
    waiting_process->pPrev = list->pTail;

    // Update the previous tail's next pointer to new process
    if (list->pTail != NULL)
    {
        list->pTail->pNext = waiting_process;
    }

    // New tail is the new process
    list->pTail = waiting_process;

    // If the list is empty, make new process the head
    if (list->pHead == NULL)
    {
        list->pHead = waiting_process;
    }

    // Increment the list count
    list->count++;

    // Increment global number of waiting processes if this is a waiting process
    if (waiting_process->child_waiting == 2)
    {
        numWaitingProc++;
    }

   return 1;
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

    // Decrement global count of waiting process if this is a waiting process
    if (poppedProc->child_waiting == 2)
    {
        numWaitingProc--;
    }

    return 1;
}