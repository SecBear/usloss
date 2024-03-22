#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <sems.h>

int start2(char *); 
int start3(char *);
int  spawn_real(char *name, int (*func)(char *), char *arg,
                int stack_size, int priority);
int  wait_real(int *status);

// Globals
process ProcTable[MAXPROC];     // Array of processes

// start2
start2(char *arg)
{
    int		pid;
    int		status;
    /*
     * Check kernel mode here.
     */

    /*
     * Data structure initialization as needed...
     */


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

/* start3 */
int start3(char *arg)
{
    // Not sure what goes here yet
}

// Grabbed this from lecture - creates a semaphore
int SemCreate(int value, int *semaphore)
{
    sysargs sa;

    CHECKMODE;  // check kernel mode?
    sa.number = SYS_SEMCREATE;  // Constant for some purpose
    sa.arg1 = (void *) value;
    usyscall(&sa);  // Invokes an interrupt - activates syscall handler - 
                    // Note: sys_vec[sys_ptr->number](sys_ptr) just calls the function at sys_vec[sys_ptr->number]. The (sys_ptr) is the parameter we're passing to the funciton call. It's just a pointer to a data structure. sys_vec is an array of function pointers.
    *semaphore = (int) sa.arg4;
    return (int) sa.arg4;
}
