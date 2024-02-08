#define DEBUG 0

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

typedef struct 
{
   /* data */
   proc_ptr pHead;
   proc_ptr pTail;
   int count;
} ProcList;


struct proc_struct {
   proc_ptr       next_proc_ptr;
   proc_ptr       prev_proc_ptr;
   proc_ptr       child_proc_ptr;
   proc_ptr       next_sibling_ptr;
   proc_ptr       prev_sibling_ptr;
   proc_ptr       parent_proc_ptr;  // Parent process 

   ProcList       children;         // List of children

   char           name[MAXNAME];     /* process's name */
   char           start_arg[MAXARG]; /* args passed to process */
   context        state;             /* current context for process */
   short          pid;               /* process id */
   int            priority;          /* process priority */
   int (* start_func) (char *);      /* function where process begins -- launch */
   char          *stack;             /* pointer to the stack */
   unsigned int   stacksize;         /* size of the stack */
   int            status;            /* READY, BLOCKED, QUIT, etc. */
   /* other fields as needed... */
   int            exitStatus;        // Exit status of the process
   int            childQuit;         // Flag indicating if a child has quit
   int            childStatus;       // Exit status of the quitting child
   int            childPid;          // PID of the quitting child
};

struct psr_bits {
        unsigned int cur_mode:1;
       unsigned int cur_int_enable:1;
        unsigned int prev_mode:1;
        unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

/* Some useful constants.  Add more as needed... */

// STATUS
#define UNUSED    -1   //defined new constant to initialize 'status' field of each proc_struct' in process table to indicate slots are available for new processess
#define EMPTY     0     
#define READY     1    //defined new constant as it represents status of a process that is ready to run 
#define BLOCKED_JOIN 2
#define RUNNING   3
#define ZOMBIE    -1
#define WAITING   -2

#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define LOWEST_PRIORITY 6  //set higher value for lower priorty in process scheduling

