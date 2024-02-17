#define DEBUG 0

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

typedef struct
{
   proc_ptr pHead;
   proc_ptr pTail;
   int count;
} ProcList; // Process List (used for Ready List, Children List, Zappers List)


struct proc_struct {
   proc_ptr       next_proc_ptr;    // Next process in Ready List
   proc_ptr       prev_proc_ptr;    // Previous process in Ready List

   proc_ptr       pNextSibling;     // Next sibling in Parent's Children List
   proc_ptr       pPrevSibling;     // Previous sibling in Parent's Children List
   proc_ptr       pParent;          // Parent process 

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
   int            exitCode;         // Exit status of the process   
   ProcList       children;         // List of children
   int            tsStart;          // Start time of process' slice
   int            cpu_time;         // How much time (in milliseconds) has the process had on CPU
   int            zapped;           // process zapped or not?
   ProcList       zappers;          // list of processes that have zapped this process
};

// Can use this to check kernel mode
struct psr_bits 
{
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
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define LOWEST_PRIORITY 6 

// STATUSES                         // Represents status of a process that is:
#define STATUS_UNUSED         -1    // unused - for initialization in startup()
#define STATUS_EMPTY          0     // empty
#define STATUS_READY          1     // ready to run 
#define STATUS_BLOCKED_JOIN   2     // blocked on join, waiting for child to return
#define STATUS_RUNNING        3     // currently running
#define STATUS_QUIT           4     // quitting
#define STATUS_BLOCKED_ZAP    5     // blocked on zap, waiting for zapped process to return
#define STATUS_CPUCALC        6     // For use with updateCpuTime() function, temporary status

// LIST TYPES
#define READY_LIST      1
#define CHILDREN_LIST   2

