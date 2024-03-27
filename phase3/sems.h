#define DEBUG2 1

typedef struct process process;


struct process {                        // A process
   int      pid;
   /* other items as needed... */
   int      status;                     // Int to hold process status (Used, Unused, etc.)
   // start mbox - do we need to create a mailbox table too? 
};


// constants
#define SYS_SEMCREATE 11                // choosing 11 at random, no purpose