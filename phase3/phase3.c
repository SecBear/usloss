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
void syscall_getpid(sysargs *args);
void syscall_semfree(sysargs *args);
void syscall_gettimeofday(sysargs *args);
void syscall_getcputime(sysargs *args);

/* ------------------------------------------------------------------------
   Global Variables
   ----------------------------------------------------------------------- */
process ProcTable[MAXPROC];     // Array of processes
semaphore SemTable[MAXSEMS];    // Array of seamphores

int numSems = 0;                // Global count of semaphores
int next_sem_id = 0;            // Integer to hold the next semaphore ID
int numWaitingProc = 0;         // Integer to hold the number of waiting processes
/* ------------------------------------------------------------------------ */


/* ------------------------------------------------------------------------
   Functions

/* ------------------------------------------------------------------------
   Name - start2()
   Purpose - Create first user-level process and wait for it to finish.
   Parameters - char *arg - Pointer to a string argument that can be used during process startup.
   Returns - int - The termination status of the last process to finish.
   Side Effects - Initializes system resources, creates and manages processes.
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
        SemTable[i].sid = NULL;
        SemTable[i].status = SEM_UNUSED;  // indicates a semaphore is freshly allocated

        // Initialize waiting list
        SemTable[i].waiting = malloc(sizeof(struct list));
        SemTable[i].waiting->pHead = NULL;  
        SemTable[i].waiting->pTail = NULL;  
        SemTable[i].waiting->count = 0;     
    }

    pid = spawn_real("start3", start3, NULL, 4*USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

} /* start2 */

/* ------------------------------------------------------------------------
   Name - syscall_spawn()
   Purpose - To create a new process in response to a SYS_SPAWN system call.
   Parameters - sysargs *args - Pointer to sysargs structure containing parameters for the new process.
   Returns - None.
   Side Effects - A new process is created and added to the process table.
   ----------------------------------------------------------------------- */
static void syscall_spawn(sysargs *args)
{
    int(*func)(char *);
    char *arg;
    int stack_size;
    int priority;
    char *name;

    // Pull out arguments from sysargs *args to pass to spawn_real()
    func = args->arg1;
    arg = args->arg2;
    stack_size = (int) args->arg3;
    priority = args->arg4; 
    name = (char *)args->arg5;

    // Spawn the process
    int kid_pid = spawn_real(name, func, arg, stack_size, priority);

    // Pack the required return values back to the caller
    args->arg1 = (void *) kid_pid;
    args->arg4 = (void *) 0;
}

/* ------------------------------------------------------------------------
   Name - spawn_real()
   Purpose - To actually create a new process using fork1, setting up necessary structures.
   Parameters - char *name - Name of the process.
                int (*func)(char *) - Function that the new process will run.
                char *arg - Argument to the function.
                int stack_size - Stack size for the new process.
                int priority - Priority of the new process.
   Returns - int - PID of the newly created process, or -1 on failure.
   Side Effects - Alters process table and potentially modifies other system state.
   ----------------------------------------------------------------------- */
int  spawn_real(char *name, int (*func)(char *), char *arg,
                int stack_size, int priority)
{
    int pid;
    int my_location;
    int kid_location;
    int result;
    int startupMbox;

    process *kidptr, *prevptr;
    my_location = getpid() % MAXPROC;

    // Fork our child using launchUserMode to set user mode and handle entry point + termination
    pid = fork1(name, launchUserMode, arg, stack_size, priority);

    // Populate Process Table with new process values
    if (pid >= 0)
    {
        int procSlot = pid % MAXPROC;               // Proc Slot in the ProcTable
        ProcTable[procSlot].pid = pid;              // Process ID
        ProcTable[procSlot].parentPid = getpid();   // Parent's PID
        ProcTable[procSlot].entryPoint = func;      // Give launchUserMode the function call
        ProcTable[procSlot].name = name;            // Process name
        ProcTable[procSlot].priority = priority;    // Process priority

        // Add process to parent's children list
        AddList(pid, ProcTable[my_location].children);

        ProcTable[procSlot].status = STATUS_RUNNING;            // Set process status to running
        MboxCondSend(ProcTable[procSlot].startupMbox, NULL, 0); // Tell process to start running (unblock in launchUserMode)
    }
    return pid;
}

/* ------------------------------------------------------------------------
   Name - launchUserMode()
   Purpose - To start the execution of a user process, setting appropriate modes.
   Parameters - char *arg - Argument to pass to the process's starting function.
   Returns - int - The result of the function run by the process.
   Side Effects - Changes process state and potentially modifies scheduler state.
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

    // Set user mode using get_psr and set_psr
    psr = psr_get();                // Get the current Process Status Register (PSR)
    psr = psr & ~PSR_CURRENT_MODE;  // Unset the current mode bit (to user mode)
    psr_set(psr);                   // Set the PSR to user mode 

    // Run the process's entry point
    result = ProcTable[procSlot].entryPoint(arg);

    // After process returns, terminate
    Terminate(result);
}

/* ------------------------------------------------------------------------
   Name - syscall_wait()
   Purpose - To block the current process until one of its children terminates.
   Parameters - sysargs *args - Pointer to sysargs structure where result will be stored.
   Returns - None.
   Side Effects - The current process may be blocked if its children are still active.
   ----------------------------------------------------------------------- */
int syscall_wait(sysargs *args)
{
    // Pull status from sysargs
    int *status = (void *)args->arg2;

    // Call wait_real and store result
    int result = wait_real(status);

    // Package the proper values to send back to the caller
    if (result >= 0)
    {
        args->arg1 = (void *)result;    // Package the pid of the terminating child
        args->arg2 = (void *)ProcTable[result % MAXPROC].termCode;  // Package the child's termination code

        // Check for children
        if (ProcTable[result % MAXPROC].children->count > 0)
        {
            args->arg4 = (void *)0;     // Process has children
        }
    }
    else if (result == -2)  // Process has no children
    {
        args->arg4 = (void *)-1;
    }
}

/* ------------------------------------------------------------------------
   Name - wait_real()
   Purpose - To wait for a child process to terminate and retrieve its status.
   Parameters - int *status - Pointer to an integer where the exit status of the child will be stored.
   Returns - int - PID of the terminated child, or -1 if there are no children.
   Side Effects - Modifies the process table and potentially the state of the process.
   ----------------------------------------------------------------------- */
extern int wait_real(int *status)
{
    // Call join with the status and return result
    int result = join(&status);
    return result;
}

/* ------------------------------------------------------------------------
   Name - syscall_terminate()
   Purpose - To terminate the current process and optionally its children.
   Parameters - sysargs *args - Pointer to sysargs structure containing the exit code.
   Returns - None.
   Side Effects - The process and potentially its children are terminated.
   ----------------------------------------------------------------------- */
void syscall_terminate(sysargs *args)
{
    // Pull the exit code out of sysargs and call terminate_real
    int exit_code = (void *) args->arg1;
    terminate_real(exit_code);
}

/* ------------------------------------------------------------------------
   Name - terminate_real()
   Purpose - Terminates the invoking process and all its children, synchronizing with its parent’s Wait system call.
   Parameters - int exit_code - Code with which the process is to be terminated.
   Returns - None.
   Side Effects - Modifies process table, may release resources.
   ----------------------------------------------------------------------- */
extern void terminate_real(int exit_code)
{
   int pid = getpid();
   process *current = &ProcTable[pid % MAXPROC];

   current->status = STATUS_TERMINATED; // Set status to terminated
   if (current->termCode != 1)  // check if termination code is already set by semfree
    {
        current->termCode = exit_code;
    }

    // Check if process has children
    if (current->children->count > 0)
    {
        // Zap each child
        process *current_child = current->children->pHead; 
        while (current_child != NULL)
        {
            // Check if the child has already terminated
            if (current_child->status == STATUS_TERMINATED)
            {
                // Remove the child from the children list and continue
                popList(current->children);
                current_child = current_child->pNext;
                break;
            }

            // If child hasn't yet terminated, zap the child and wake it up
            zap(current_child->pid);
            MboxCondReceive(current_child->privateMbox, NULL, 0);
            popList(current->children);     // Child should be done now, so pop it
            current_child = current_child->pNext;
        }
    }

    // Quit (terminate) this process
    quit(exit_code);
}


/* ------------------------------------------------------------------------
   Name - syscall_semcreate()
   Purpose - To create a new semaphore with an initial value.
   Parameters - sysargs *args - Pointer to sysargs structure containing the initial semaphore value.
   Returns - None.
   Side Effects - Modifies the semaphore table and potentially affects process synchronization.
   ----------------------------------------------------------------------- */
void syscall_semcreate(sysargs *args) 
{   
    // Pull the initial semaphore value out of sysargs and call semcreate_real
    int init_value = (int)(long)args->arg1;
    int sem_id = semcreate_real(init_value);

    // Package the result back for the caller
    if (sem_id >= 0) 
    {
        // Success: Return semaphore ID and set result to 0
        args->arg1 = (void *)(long)sem_id; // Correctly returning semaphore ID
        args->arg4 = (void *)0;            // Indicating success
    } 
    else 
    {
        // Failure: Indicate failure in creating a semaphore
        args->arg1 = (void *)(long)-1;     // Semaphore not created, so returning -1
        args->arg4 = (void *)(long)-1;     // Indicate failure
    }
}

/* ------------------------------------------------------------------------
   Name - semcreate_real()
   Purpose - Allocates a semaphore and initializes it with the specified value.
   Parameters - int init_value - Initial value of the semaphore.
   Returns - int - ID of the newly created semaphore, or -1 on failure.
   Side Effects - Modifies semaphore table.
   ----------------------------------------------------------------------- */
int semcreate_real(int init_value) 
{

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
    SemTable[semID].mutex = MboxCreate(1,0);    // Create semaphore's mutex
    
    numSems++;  // Increment max number of sems
    
    return semID;
}

/* ------------------------------------------------------------------------
   Name - GetNextSemID()
   Purpose - To find a free semaphore ID to use for a new semaphore.
   Parameters - None.
   Returns - int - The next free semaphore ID, or -1 if no IDs are available.
   Side Effects - None directly, but impacts how semaphores are allocated.
   ----------------------------------------------------------------------- */
int GetNextSemID()
{
   int new_sem_id = -1;                 // Initialize new mbox id to -1
   int semSlot = next_sem_id % MAXMBOX; // Assign new mailbox to next_mbox_id mod MAXMBOX (to wrap around to 1, 2, 3, etc. from max)

   if (numSems < MAXSEMS)               // If there's room for another process
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
   Name - syscall_semv()
   Purpose - To perform a V (signal) operation on the specified semaphore.
   Parameters - sysargs *args - Pointer to sysargs structure containing the semaphore ID.
   Returns - None.
   Side Effects - May wake up a process waiting on the semaphore.
   ----------------------------------------------------------------------- */
void syscall_semv(sysargs *args)
{
    int semID = (int)args->arg1;    // parse argument (semaphore ID)
    semv_real(semID);               // call semv_real with semID
}

/* ------------------------------------------------------------------------
   Name - semv_real()
   Purpose - Increments the semaphore value and wakes up any blocked process.
   Parameters - int semID - ID of the semaphore to signal.
   Returns - int - Always returns 0 (success).
   Side Effects - Modifies the state of the semaphore and potentially the state of a process.
   ----------------------------------------------------------------------- */
int  semv_real(int semID)
{
    semaphore *sem = &SemTable[semID];       // Grab the semaphore from the SemTable
    int pid = getpid();                      // Get the pid of current process
    process *current_proc = &ProcTable[pid]; // Get the current process

    // Increment the value
    MboxSend(sem->mutex, NULL, 0);       // Get the mutex
    sem->value++;                        // Increment the value
    MboxReceive(sem->mutex, NULL, 0);    // Release the mutex

    // Check for blocked processes on the semaphore because of P operation
    if (sem->waiting->count > 0)        // If processes are waiting (blocked)
    {  
        // Traverse through waitlist to find the process
        process *pNext = sem->waiting->pHead;
        while (pNext != NULL)   
        {   
            popList(sem->waiting);      // Remove process from waiting list
            sem->status = SEM_USED;     // Set the semaphore status to used again
            MboxCondSend(pNext->privateMbox, NULL, 0); // Wake up the blocked process
            break;
        }
    }
    return 0; // success
}

/* ------------------------------------------------------------------------
   Name - syscall_semp()
   Purpose - To perform a P (wait) operation on the specified semaphore.
   Parameters - sysargs *args - Pointer to sysargs structure containing the semaphore ID.
   Returns - None.
   Side Effects - May block the calling process if the semaphore value is zero.
   ----------------------------------------------------------------------- */
void syscall_semp(sysargs *args)
{
    // Pull the semaphore ID from sysargs and call semp_real
    int semID = (int)args->arg1; 
    semp_real(semID);               
}

/* ------------------------------------------------------------------------
   Name - semp_real()
   Purpose - Decrements the semaphore value, possibly blocking the calling process.
   Parameters - int semID - ID of the semaphore to wait on.
   Returns - int - Always returns 0 (success).
   Side Effects - Modifies the state of the semaphore and potentially the state of the calling process.
   ----------------------------------------------------------------------- */
int  semp_real(int semID)
{
    semaphore *sem = &SemTable[semID];  // Get the semaphore
    int pid = getpid();                 // Get the pid of current process
    process *process = &ProcTable[pid]; // Get the current process

    // If the semaphore value > 0
    if (sem->value > 0)
    {
        // Decrement the semaphore
        MboxSend(sem->mutex, NULL, 0);    // Obtain the mutex
        sem->value--;                     // Decrement the semaphore value
        MboxReceive(sem->mutex, NULL, 0); // Release mutex
    }
    else
    {
        // Otherwise, add the process to waiting list, effectively blocking it (we're trying to decrement below 0)
        AddList(pid, sem->waiting);                  // Add process to wait list
        MboxReceive(process->privateMbox, NULL, 0);  // Block by receiving on the current process's private mailbox

        // After waking up
        // Check if we've been free'd
        if (sem->status == SEM_FREE)
        {   
            process->termCode = 1;        // Set status to 1 - there are processes blocked on the semaphore
            terminate_real(pid);         // Terminate
        }
        MboxSend(sem->mutex, NULL, 0);    // Obtain the mutex
        sem->value--;                     // Decrement the semaphore
        MboxReceive(sem->mutex, NULL, 0); // Release mutex
    }
    return 0;    // success
}

/* ------------------------------------------------------------------------
   Name - syscall_semfree()
   Purpose - To free a semaphore and wake up all processes waiting on it.
   Parameters - sysargs *args - Pointer to sysargs structure containing the semaphore ID.
   Returns - None.
   Side Effects - Modifies the semaphore table and affects process synchronization.
   ----------------------------------------------------------------------- */
void syscall_semfree(sysargs *args)
{
    // Pull the semaphore ID out of the sysargs and call semfree_real
    int semID = args->arg1;             
    int result = semfree_real(semID);  

    // Check result and package the correct value back to the caller
    if (result == -1)
    {
        // Indicating semaphore handle is invalid
        args->arg4 = (void *)(long)-1;  
    }
    else if (result == 0)
    {
        // Indicating success
        args->arg4 = (void *)0;         
    }
    else if (result == 1)
    {
        // Indicating processes were blocked on semaphore
        args->arg4 = (void *)1;         
    }
}

/* ------------------------------------------------------------------------
   Name - semfree_real()
   Purpose - Frees the specified semaphore and handles any associated cleanup.
   Parameters - int semID - ID of the semaphore to be freed.
   Returns - int - 0 if no processes were waiting, 1 if processes were waiting, -1 on error.
   Side Effects - Modifies semaphore table and may affect processes waiting on the semaphore.
   ----------------------------------------------------------------------- */
int semfree_real(int semID)
{
    semaphore *sem = &SemTable[semID];  // Get the semaphore
    int pid = getpid();                 // Get the pid of current process
    process *proc = &ProcTable[pid];    // Get the current process
    int result = 0;                     // Initialize the result value

    // Error checking
    if (sem == NULL)
    {
        return -1;
    }

    // Obtain the mutex
    MboxSend(sem->mutex, NULL, 0);
    sem->status = SEM_FREE;     // Set semaphore status to free

    // Check for any processes waiting on the semaphore
    if (sem->waiting->count > 0)
    {
        // Terminate them
        process *current = sem->waiting->pHead;
        while (current != NULL)
        {
            popList(sem->waiting);                          // Remove process from the waiting list
            MboxCondSend(current->privateMbox, NULL, 0);    // Wake up the process (should terminate with above status)
            current = current->pNext;                       // Continue to next waiting process
        }
        result = 1;
    }

    // Set status back to unused 
    sem->status = SEM_UNUSED;   

    // Release the mutex
    MboxReceive(sem->mutex, NULL, 0);

    // Decrement global semaphore count
    --numSems;  

    return result;
}

/* ------------------------------------------------------------------------
   Name - syscall_handler()
   Purpose - Dispatch system call requests to appropriate system call functions.
   Parameters - int dev - Device type that generated the interrupt (not used here).
                void *punit - Pointer to a sysargs structure holding system call arguments.
   Returns - None.
   Side Effects - Depending on the system call, various system resources are manipulated.
   ----------------------------------------------------------------------- */
void syscall_handler(int dev, void *punit) 
{   
    // Check ther we're in kernel mode
    check_kernel_mode("sys_handler");
    
    // Pull the arguments from the punit parameter
    sysargs *args = (sysargs*)punit;

    if (dev != SYSCALL_INT) {
        halt(1); // Only proceed if the interrupt is a syscall interrupt
    }

    // Check for invalid sys number
    if (args->number >= MAXSYSCALLS)
    {
        printf("syscall_handler(): sys number %d is wrong. Halting...\n", args->number);
        halt(1);
    }
    else if (args == NULL || args->number < 0) {
        nullsys3(args);
    } else
    {   
        // Call the relevant system call handler function
        sys_vec[args->number](args);
    }
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
   Name - syscall_gettimeofday()
   Purpose - To retrieve the current system time.
   Parameters - sysargs *args - Pointer to sysargs structure where the time will be stored.
   Returns - None.
   Side Effects - None directly, but provides time information to calling process.
   ----------------------------------------------------------------------- */
void syscall_gettimeofday(sysargs *args)
{
    // Call sys_clock() to get the current time 
    //  and package the result back to the caller
    int result = sys_clock();
    args->arg1 = (void *)result;
}

/* ------------------------------------------------------------------------
   Name - syscall_getcputime()
   Purpose - To retrieve the CPU time used by the current process.
   Parameters - sysargs *args - Pointer to sysargs structure where the CPU time will be stored.
   Returns - None.
   Side Effects - None directly, but provides CPU usage information to the calling process.
   ----------------------------------------------------------------------- */
void syscall_getcputime(sysargs *args)
{
    // Call readtime() to get the current process's cpu time and 
    //  package result back to caller
    int result = readtime();
    args->arg1 = (void *)result;
}

/* ------------------------------------------------------------------------
   Name - syscall_getpid()
   Purpose - To retrieve the process ID of the calling process.
   Parameters - sysargs *args - Pointer to sysargs structure where the PID will be stored.
   Returns - None.
   Side Effects - None directly, but provides PID information to the calling process.
   ----------------------------------------------------------------------- */
void syscall_getpid(sysargs *args)
{
    // Call getpid() to get the current process's PID
    //  and package result back to caller
    int pid = getpid();
    args->arg1 = pid;
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