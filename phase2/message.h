#define DEBUG2 1

typedef struct mail_slot *slot_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;

struct mailbox {
   int           mbox_id;
   /* other items as needed... */
   // Array or linked list of slots? Array of linked lists?
   // List of processes that are waiting to receive 
};

struct mail_slot {
   int       mbox_id;
   int       status;
   /* other items as needed... */
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

// Waiting list
struct WAITINGPROC
{
   int pid;                      // pid of waiting process
   // Add Process info here
   struct next_proc *next_ptr;   // pointer to the next process that's waiting
};

// Process table? (use getnextpid() % MAXPROC to get new pids for new proc table)