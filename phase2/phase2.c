/* ------------------------------------------------------------------------
   phase2.c
   Applied Technology
   College of Applied Science and Technology
   The University of Arizona
   CSCV 452

   Bryce Thorpe & Vivek Madala

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1(char *);
extern int start2(char *);
void check_kernel_mode(char string[]);
void disableInterrupts();
static void enableInterrupts();
int MboxCreate(int slots, int slot_size);
int MboxSend(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondSend();
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondReceive();
int AddToWaitList(int mbox_id, int status, void *msg_ptr, int msg_size);
int GetNextMboxID();
slot_ptr GetNextReadySlot(int mbox_id);
void SlotListInit(mail_box *mbox, int slots, int slot_size);
int GetNextSlotID();
void clock_handler2();
void disk_handler();
void term_handler();
void syscall_handler();
static void nullsys();

/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// Mailboxes & slots
mail_box MailBoxTable[MAXMBOX];   // Array of 2000 mailboxes
mail_slot MailSlotTable[MAXSLOTS]; // Array of 2500 Mail slot pointers (NOTE: slot array/table is not the slot list)
mbox_proc ProcTable[MAXPROC];     // Array of processes

/* array of mail box processes (proc table) */
// static struct mbox_proc MboxProcs[MAXSLOTS]; // NOTE: use `i = getpid()%MAXPROC` to get the next pid

int slot_count = 0; // Integer to keep track of total number of slots

unsigned int next_mbox_id = 0; // The next mbox_id to be assigned
unsigned int next_slot_id = 0; // The next slot_id to be assigned
int numMbox = 0;               // Number of currently active mailboxes
int numSlot = 0;               // Number of currently active slots

int clock_count = 0;    // Count to keep track of clock_handler calls
int waiting_for_io = 0; // Count to keep track of processes waiting for I/O

// Interrupt Mailboxes
int clock_mbox;
int disk_mbox[2];
int term_mbox[4];
// int clock_mbox[];

void (*sys_vec[MAXSYSCALLS])(sysargs *args); // for system call handler

/* -------------------------- Functions -----------------------------------
  Below I have code provided to you that calls

  check_kernel_mode
  enableInterrupts
  disableInterupts

  These functions need to be redefined in this phase 2,because
  their phase 1 definitions are static
  and are not supposed to be used outside of phase 1.  */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
   int kid_pid, status;

   if (DEBUG2 && debugflag2)
   {
      console("start1(): at beginning\n");
   }

   check_kernel_mode("start1");

   /* Disable interrupts */
   disableInterrupts();

   /* Initialize the mail box table, slots, & other data structures. */

   // Mail box table
   for (int i = 0; i < MAXMBOX; i++)
   {
      // Status, ID and available messages
      MailBoxTable[i].mbox_id = STATUS_UNUSED;
      MailBoxTable[i].status = STATUS_EMPTY;
      MailBoxTable[i].available_messages = STATUS_EMPTY;
      MailBoxTable[i].zero_slot = STATUS_UNUSED;
   }

   // Slot table
   for (int i = 0; i < MAXSLOTS; i++)
   {
      // Status, mailbox ID, slot ID, status, next and prev slot
      MailSlotTable[i].status = STATUS_UNUSED;
      MailSlotTable[i].mbox_id = STATUS_UNUSED;
      MailSlotTable[i].slot_id = i;
      MailSlotTable[i].message[MAX_MESSAGE] = '\0';
      MailSlotTable[i].next_slot = NULL;
      MailSlotTable[i].prev_slot = NULL;
   }

   // Proc table
   for (int i = 0; i < MAXPROC; i++)
   {
      // PID, status, Message
      ProcTable[i].status = STATUS_EMPTY;
      ProcTable[i].pid = STATUS_UNUSED;
      ProcTable[i].message[MAX_MESSAGE] = "\0";
   }

   /* Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */

   int_vec[CLOCK_DEV] = clock_handler2;    // clock handler
   int_vec[DISK_DEV] = disk_handler;       // disk handler
   int_vec[TERM_DEV] = term_handler;       // terminal handler
   int_vec[SYSCALL_INT] = syscall_handler; // System call handler

   clock_mbox = MboxCreate(0, 0); // clock mailbox

   for (int i = 0; i < 2; i++) // disk mailboxes
   {
      disk_mbox[i] = MboxCreate(0, 0); // I/O mailboxes for disks
   }

   for (int i = 0; i < 4; i++) // Term mailboxes
   {
      term_mbox[i] = MboxCreate(0, 0); // I/O mailboxes for terminals
   }

   for (int i = 0; i < MAXSYSCALLS; i++)
   {
      sys_vec[i] = nullsys; // initialize every system call handler as nullsys
   }

   enableInterrupts();

   /* Create a process for start2, then block on a join until start2 quits */
   if (DEBUG2 && debugflag2)
      console("start1(): fork'ing start2 process\n");
   kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
   if (join(&status) != kid_pid)
   {
      console("start2(): join returned something other than start2's pid\n");
   }

   return 0;
} /* start1 */

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

void disableInterrupts()
{
   /* turn the interrupts OFF iff we are in kernel mode */
   check_kernel_mode("disableInterrupts");

   /* We ARE in kernel mode */
   psr_set(psr_get() & ~PSR_CURRENT_INT);

} /* disableInterrupts */

static void enableInterrupts()
{
   int currentPsr = psr_get();                         // Get current psr
   int interruptEnable = currentPsr | PSR_CURRENT_INT; // Set the interrupt enable bit to ON (0x2)
   psr_set(interruptEnable);                           // Set psr to new psr
}

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
   int mbox_id;
   // Check kernel mode?

   // Check for simple errors
   if (slots < 0 || slot_size < 0 || slots + slot_count > MAXSLOTS) // If we're trying to create too many slots
   {
      return -1;
   }

   // Similar to what we did with proc table
   // MAXMBOX constant = max number of mailboxes

   // Look through all mailboxes, when found one, return it,
   // mbox_id % MAXMBOX to wrap around
   mbox_id = GetNextMboxID();

   // Define mailbox in the MailBoxTable
   // struct mailbox mbox = MailBoxTable[mbox_id];

   // Single slot mailbox for mutex (only one message in mailbox at a time)
   // multi-slot mailboxes can be used to implement semaphore (>=0)
   if (slots == 0) // If this is a zero slot mailbox,
   {
      MailBoxTable[mbox_id].zero_slot = 1; // Zet zero_slot flag to 1
   }
   else // If not,
   {
      MailBoxTable[mbox_id].zero_slot = 0;                    // Set zero_slot flag to 0
      SlotListInit(&MailBoxTable[mbox_id], slots, slot_size); // Initialize slots & slot list
   }

   WaitingListInit(&MailBoxTable[mbox_id]); // Initialize waiting list

   MailBoxTable[mbox_id].mbox_id = mbox_id;    // Update mailbox ID
   MailBoxTable[mbox_id].status = STATUS_USED; // Update mailbox status
   numMbox++;                                  // Increment number of mailboxes

   return mbox_id;
} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) // atomic (no need for mutex or semaphore, etc. note: interrupts are disabled)
{
   int needToUnblock = 0;
   int pid = -1;

   check_kernel_mode("MboxSend\n");

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || msg_ptr == NULL)
   {
      // Error message here
      return -1;
   }

   // Get the mailbox from the mail box table
   mail_box *mbox = &MailBoxTable[mbox_id];

   // Is anyone waiting? (check waiting list and wake up the first process to start waiting)
   // if so, we need to copy that data into the mailbox and unblock the process that's waiting
   if (mbox->waiting_list->count > 0) // Is anyone waiting on this mailbox?
   {
      waiting_proc_ptr current = mbox->waiting_list->pHead; // Start with the head of the waiting list
      while (current != NULL)                               // Loop through Waiting list
      {
         if (current->process->status == STATUS_WAIT_RECEIVE) // If this process is waiting to recieve
         {
            // If this is a zero slot mailbox, send process directly
            // Do something
            // else, send the message to next available mailbox and unblock proc
            pid = current->process->pid;
            // Remove process from waiting list
            popWaitList(mbox_id);
            // set the flag to unblock proc once message sent
            needToUnblock = 1;
            // only if zero slot mailbox Direct send to the receiver who's waiting
            // memcpy(current->process->message, msg_ptr, msg_size); // Using memcpy instead of strcpy in the case of message not being null-terminate
            // return 1; // success!
         }
         current = current->pNext; // Check the next wating process
      }
   } // Else, nobody is waiting

   // If slot is available in this mailbox, allocate a slot from your mail slot table and send it
   if (mbox->slot_list->count < mbox->slot_list->slot_count) // slot is available for allocation
   {  
      // Get next slot id and assign this slot to the table
      int slot_id = GetNextSlotID();
      slot_ptr new_slot = &MailSlotTable[slot_id];
      // Initialize other fields
      new_slot->mbox_id = mbox->mbox_id;
      new_slot->status = STATUS_USED;

      // Copy the message into the slot
      memcpy(new_slot->message, msg_ptr, msg_size);

      // Link the new slot to the mailbox's linked list of slots
      new_slot->next_slot = NULL;                       // New slot's pNext is NULL
      new_slot->prev_slot = mbox->slot_list->tail_slot; // Old tail is new slot's pPrev
      if (mbox->slot_list->head_slot == NULL)
      {                                         // If the list's head is NULL,
         mbox->slot_list->head_slot = new_slot; // New slot is the new head
      }
      else
      {                                                    // Otherwise,
         mbox->slot_list->tail_slot->next_slot = new_slot; // New slot is the tail's pNext
      }
      mbox->slot_list->tail_slot = new_slot; // New slot is now the tail
      mbox->slot_list->count++;              // Increment the slot list count
      numSlot++;                             // increment global list of slots

      // Increment the mailbox's available_messages
      mbox->available_messages++;

      // remember to deallocate on receive

      if (needToUnblock)
      {
         unblock_proc(pid);
      }

      return 0;
   }

   // No available slot was found,
   // block the sender, add sender to waiting list
   AddToWaitList(mbox_id, STATUS_WAIT_SEND, msg_ptr, msg_size); // Add this process to waiting list, waiting to send
   block_me(STATUS_WAIT_SEND);                                  // Sender is waiting to send

   return 0;

} /* MboxSend */

int MboxCondSend(); // non-blocking send

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
  ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size) // atomic (no need for mutex or semaphore, etc. note: interrupts are disabled)
{
   int zero_slot = 0;

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || msg_ptr == NULL)
   {
      // Error message here
      return -1;
   }

   // Get the mailbox
   mail_box *mbox = &MailBoxTable[mbox_id];

   // Is this a zero slot?
   if (mbox->zero_slot)
   {
      // do something
      printf("we're a zero slot! - do something with me!!! - returning 0\n");
      return 0;
   }

   // is somebody waiting to send? (receieve from them directly)
   if (mbox->waiting_list->count > 0)
   {
      // Find the process already waiting, see if they're waiting to send
      waiting_proc_ptr current = mbox->waiting_list->pHead; // Start with the head of the waiting list
      while (current != NULL)                               // Loop through Waiting list
      {
         if (current->process->status == STATUS_WAIT_SEND) // If this process is waiting to send
         {
            popWaitList(mbox_id);       // remove process from waiting list

            // Get the message from the sender directly
            memcpy(msg_ptr, current->process->message, msg_size); // Copy the message including null terminator into receiver's buffer (is this the right buffer?)

            // Clean the buffer
            memset(current->process->message, 0, MAX_MESSAGE); // Zero out the message buffer

            // unblock the waiting process
            unblock_proc(current->process->pid); // Unblock already waiting process

            // Enable/Disable interrupts?

            // Return 1 (success)
            return 1;
         }
      }
      current = current->pNext; // Check the next wating process
   }

   // block until message is here (using semaphores)
   if (mbox->available_messages <= 0) // If no messages in this mailbox,
   {
      AddToWaitList(mbox_id, STATUS_WAIT_RECEIVE, NULL, -1); // Add to Waiting list of processes to recieve a message?
      block_me(STATUS_WAIT_RECEIVE);                         // Block with status waiting to receive
   }

   // This process is the one to receive
   // Grab the next available slot, get it's message and clean the mailbox slot
   slot_ptr slot = GetNextReadySlot(mbox_id); // Get the next slot with a message
   char *message = slot->message;            // Pull its message

   // Put the message from the mailbox slot into the receiver's buffer
   memcpy(msg_ptr, message, msg_size); // Copy the message including null terminator

   // Clean / Deallocate the slot
   void CleanSlot(slot, mbox);

   // disable/enable interrupts?

   return 1; // success
} /* MboxReceive */

int MboxCondReceive(); // non-blocking receive

/*
MboxRelease()
{
   // Mark mailbox to be released as being relesased

   // Reclaim the mail slots allocated for the mailbox so they can be reused

   // Releaser checks if there are processes blocked on the mailbox
      // How many processes are blocked?
      // Unblock each process (call unblock_proc)
      // block itself (call block_me) if there exists any process previously blocked on the mailbox that
         hasn't returned from either sending or recieving operation
}*/

int check_io()
{
   // return 1 if at least one process is blocked on an I/O mailbox (including clock mbox)
   if (waiting_for_io > 0)
   {
      return 1;
   }
   /* IF ABOVE DOESN'T WORK, USE WAITING LIST FOR I/O MAILBOXES?
   // Check clock mailbox
   if (MailBoxTable[clock_mbox].waiting_list->count > 0) // Someone waiting on clock mailbox
   {
      // Check if we're waiting on a receive - I/O mailboxes don't wait to send
      return 1;
   }

   // Check disk mailbox
   for (int i = 0; i < 2; i++)
   {
      if (MailBoxTable[disk_mbox[i]].waiting_list->count > 0)  // Someone waiting on disk mailbox
      {  // Check if we're waiting on a receive - I/O mailboxes don't wait to send
         return 1;
      }
   }

   // Check term mailbox
   for (int i = 0; i < 4; i++)
   {
      if (MailBoxTable[term_mbox[i]].waiting_list->count > 0)  // Someone waiting on a term mailbox
      {
         // Check if we're waiting on a receive - I/O mailboxes don't wait to send
         return 1;
      }
   } */

   // return 0 otherwise
   return 0;
}

// Get the next ready mailbox ID and return it
int GetNextMboxID()
{
   int new_mbox_id = -1;                  // Initialize new mbox id to -1
   int mboxSlot = next_mbox_id % MAXMBOX; // Assign new mailbox to next_mbox_id mod MAXMBOX (to wrap around to 1, 2, 3, etc. from max)

   if (numMbox < MAXMBOX) // If there's room for another process
   {
      // Loop through until we find an empty slot
      while (MailBoxTable[mboxSlot].status != STATUS_EMPTY && mboxSlot != next_mbox_id)
      {
         next_mbox_id++;
         mboxSlot = next_mbox_id % MAXMBOX;
      }

      if (MailBoxTable[mboxSlot].status == STATUS_EMPTY)
      {
         new_mbox_id = next_mbox_id;                  // Assigns new_mbox_id to current next_mbox_id value
         next_mbox_id = (next_mbox_id + 1) % MAXMBOX; // Increment next_mbox_id for the next search
      }
   }

   return new_mbox_id;
}

// Get the next ready slot ID and return it
int GetNextSlotID()
{
   int new_slot_id = -1;
   int slot = next_slot_id % MAXSLOTS;
   int initialSlot = slot;
   
   if (numSlot < MAXSLOTS)  // If there's room for another slot
   {
      // Loop through until we find an empty slot or reach the initial slot
      while (MailSlotTable[slot].status != STATUS_EMPTY && slot != initialSlot)
      {
         next_slot_id = (next_slot_id + 1) % MAXSLOTS; // Increment next_slot_id for the next search
         slot = next_slot_id % MAXSLOTS;              // Update slot index
      }

      if (MailSlotTable[slot].status < 1)             // if status is unused or empty
      {
         new_slot_id = slot;   // Assigns new_slot_id to current slot
         next_slot_id = (next_slot_id + 1) % MAXSLOTS; // Increment next_slot_id for the next search
      }
   }

   return new_slot_id;
}

// AddToList functions to add an item to the end of a linked list

// Add the current process to a mailbox's list of watiing processes along with its message if it's waiting to send
int AddToWaitList(int mbox_id, int status, void *msg_ptr, int msg_size)
{
   mail_box mbox = MailBoxTable[mbox_id]; // Get mailbox
   int pid = getpid();                    // Get process id - not sure how to access processes yet
   waiting_list list = mbox.waiting_list; // Get waiting list

   // Add process to mailbox's waiting list
   if (pid == NULL)
   {
      // Invalid process pointer
      return 0;
   }

   // Check if the process is already on the list
   waiting_proc_ptr current = list->pHead;
   if (current != NULL)
   {
      if (current->process->pid == pid)
      {
         // Process is already in the list
         return 0;
      }
      current = current->pNext; // Move to next process
   }

   // Allocate space for new waiting process
   waiting_proc_ptr waiting_process = (waiting_proc_ptr)malloc(sizeof(waiting_proc)); // Allocate memory for the slot

   // Assign process information
   waiting_process->process = &ProcTable[pid];
   waiting_process->process->pid = pid;
   waiting_process->process->status = status;
   // if we're storing a message:
   if (msg_size != -1) // msg_size of -1 means no message
   {
      waiting_process->process->msg_size = msg_size;                // Store message size for later
      memcpy(waiting_process->process->message, msg_ptr, msg_size); // Copy message for later
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

   return 1;
}

// PopList functions to pop the first item added to the linked list (head)

// Pops the head process from the waiting list and returns the waiting_proc_ptr
int popWaitList(int mbox_id)
{
   check_kernel_mode("popWaitList\n");

   // Get waiting list
   waiting_list list = MailBoxTable[mbox_id].waiting_list;

   // Check if list is empty
   if (list->count == 0)
   {
      return NULL;
   }

   // Get the oldest item and replace list's head
   waiting_proc_ptr poppedProc = list->pHead; // get the head of the list (oldest item)
   list->pHead = poppedProc->pNext;           // update the head to the next process

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

   return 1;
}

// Get the next ready slot with message in a mailbox and clean the slot
slot_ptr GetNextReadySlot(int mbox_id)
{
   mail_box mbox = MailBoxTable[mbox_id]; // Get the mail box
   char *message = NULL;

   // Check that mail box has a slot available
   if (mbox.available_messages <= 0)
   {
      // If it doesn't, block_me and add to waiting list?
      block_me(1);
   }

   slot_ptr current = mbox.slot_list->head_slot;
   while (current != NULL)
   {
      // Iterate through each slot and check if there's an available message
      if (current->message[0] != '\0')
      {
         // if there is, set slot status to empty and return it
         current->status = STATUS_EMPTY; // slot will be cleaned outside this
         return current;                 // return the slot poibnter
      }
      current = current->next_slot; // If not, on to the next slot
   }

   printf("ERROR: GetNextReadySlot: no slot available?? please investigate\n");
   halt(1);
}

/* LIST INITIALIZATION FUNCTIONS */

// Initializes the slot list of a mailbox but does not assign the slots yet
// Takes a pointer to the mailbox, the number and size of slots
void SlotListInit(mail_box *mbox, int slots, int slot_size)
{
   int mbox_id;

   // Allocate memory for slot_list
   mbox->slot_list = (struct slot_list *)malloc(sizeof(struct slot_list));
   memset(mbox->slot_list, 0, sizeof(slot_list)); // Initialize the slot list with 0
   mbox->slot_list->head_slot = NULL;             // Initialize head slot pointer to NULL
   mbox->slot_list->tail_slot = NULL;             // Initialize tail slot pointer to NULL
   mbox->slot_list->slot_count = slots;           // Save slot count
   mbox->slot_list->slot_size = slot_size;        // Save slot size
   mbox->slot_list->mbox_id = mbox_id;            // Save mbox ID
   mbox->slot_list->count = 0;                    // Initialize count to 0

   /*
   // Initialize mailbox slots and link
   for (int i = 0; i < slots; ++i)
   {
      slot_ptr mbox_slot = (slot_ptr)malloc(sizeof(struct mail_slot)); // Allocate memory for the slot

      if (mbox_slot == NULL)
      {
         slot_ptr current_slot = mbox->slot_list->head_slot;
         while (current_slot != NULL)
         {
            // Cleanup previously allocated slots
            slot_ptr next_slot = current_slot->next_slot;
            free(current_slot);
            current_slot = next_slot;
         }
         free(mbox->slot_list); // Cleanup slot list
         return -1;
      }

      mbox_slot->mbox_id = mbox_id;          // Assign slot's mbox id
      //mbox_slot->slot_id = GetNextSlotID();  // Assign slot's slot_id - We allocate / deallocate slots their slot ID when we send/receive. This way
      mbox_slot->status = STATUS_EMPTY;      // Assign slot's status

      // Link the slot
      mbox_slot->next_slot = NULL;  // Set next to NULL
      mbox_slot->prev_slot = mbox->slot_list->tail_slot; // Set prev to tail

      // Update pointers in the slot list
      if (mbox->slot_list->head_slot == NULL)
      {
         // If this is the first slot in the list
         mbox->slot_list->head_slot = mbox_slot;    // Assign current to head
      }
      else
      {
         // Add the slot to the end of the list
         mbox->slot_list->tail_slot->next_slot = mbox_slot;  // Assign current to previous tail's next
      }
      mbox->slot_list->tail_slot = mbox_slot; // Update tail
      mbox->slot_list->count++;               // Increment count of slots
   } */
}

// Initialize mailbox's waiting list
void WaitingListInit(mail_box *mbox)
{
   int mbox_id;

   // Allocate memory for waiting_list
   mbox->waiting_list = (struct waiting_list *)malloc(sizeof(struct waiting_list));

   // Initialize values
   memset(mbox->waiting_list, 0, sizeof(struct waiting_list)); // Initialize the waiting list with 0s
   mbox->waiting_list->pHead = NULL;
   mbox->waiting_list->pTail = NULL;
   mbox->waiting_list->count = 0;
   mbox->waiting_list->mbox_id = mbox_id;
}

/* HANDLER FUNCTIONS */

// Clock handler
void clock_handler2(int dev, void *pUnit)
{
   check_kernel_mode("clock handler\n");
   // Clock interrupt has occurred

   // Error check: is the device the correct device? Is the unit number in the correct range?

   clock_count++; // Increment clock count

   if (clock_count == 5) // If this is the 5th interrupt,
   {
      clock_count = 0; // Reset clock count
      // MBoxCondSend();   // Conditionally send to the clock I/O mailbox
   }

   // time slice (check if time is up, if so, make ready and dispatch)
   time_slice();
}

// Disk handler
void disk_handler(int dev, void *punit)
{
   int status;
   int result;
   int unit = (int)punit;

   check_kernel_mode("disk handler\n");

   // Error checks is the device the correct device? Is the unit number in the correct range?
   if (unit < 0 || unit > 1)
   {
      halt(1);
   }

   // Read the device status register by using the USLOSS device_input function. You need to call this function
   device_input(DISK_DEV, unit, &status);

   // Conditionally send the content of the status register to the appropriate I/O mailbox (zero slot mailbox)
   // Cond-send is used so that low-level device handler is never blocked on the mailbox
   // MboxCondSend(disk_mbox[unit], &status, sizeof(status));  // Need to implement disk_mbox
   //  should do some checking on the returned result value
}

// Terminal handler
void term_handler(int dev, void *punit)
{
   int status;
   int result;
   int unit = (int)punit;

   check_kernel_mode("terminal handler\n");

   // Error checks is the device the correct device? Is the unit number in the correct range?
   if (unit < 0 || unit > 3)
   {
      halt(1);
   }

   // Read the device status register by using the USLOSS device_input function. You need to call this function
   device_input(TERM_DEV, unit, &status);

   // Conditionally send the content of the status register to the appropriate I/O mailbox (zero slot mailbox)
   // Cond-send is used so that low-level device handler is never blocked on the mailbox
   // MboxCondSend(term_mbox[unit], &status, sizeof(status));  // Need to implement term_mbox
   //  should do some checking on the returned result value
}

// Syscall Handler
void syscall_handler(int dev, void *punit)
{
   check_kernel_mode("syscall handler\n");

   int unit = (int)punit;

   sysargs *sys_ptr;
   sys_ptr = (sysargs *)unit;

   // Sanity check: if the interrupt is not SYSCALL_INT, halt(1)
   if (dev != SYSCALL_INT)
   {
      halt(1);
   }
   // check what system - if the call is not in the range between 0 and MAXSYSCALLS, halt(1)
   if (unit < 0 || unit > MAXSYSCALLS)
   {
      halt(1);
   }

   // now it is time to call the appropriate system call handler
   sys_vec[sys_ptr->number](sys_ptr);
}

// nullsys for system call handler
static void nullsys(sysargs *args)
{
   printf("nullsys(); Invalid syscall %d. Halting...\n", args->number);
   halt(1);
} /* nullsys */

// Waitdevice
int waitdevice(int type, int unit, int *status)
{
   int result = 0;

   // Sanity checks
   // More code could be inserted before the below checking
   switch (type)
   {
   case CLOCK_DEV:
      // More code for communication with clock device
      waiting_for_io++;
      result = MboxReceive(clock_mbox, status, sizeof(int));
      break;

   case DISK_DEV:
      waiting_for_io++;
      result = MboxReceive(disk_mbox[unit], status, sizeof(int));
      break;

   case TERM_DEV:
      // More logic
      waiting_for_io++;
      result = MboxReceive(term_mbox[unit], status, sizeof(int));
      break;

   default:
      printf("waitdevice(): bad type (%d). Halting...\n", type);
      halt(1);
   }

   if (result == -3)
   {
      // we're zapped
      return -1;
   }
   else
   {
      waiting_for_io--;
      return 0;
   }
} /* Waitdevice */

void CleanSlot(slot_ptr slot, mail_box *mbox)
{
   // Clean the slot's message buffer
   memset(slot->message, 0, MAX_MESSAGE);

   // Set the slot's status and mbox ID to empty / unused
   slot->status = STATUS_EMPTY;
   slot->mbox_id = STATUS_UNUSED;

   // Decrement the count of slots in the mailbox's slot list
   mbox->slot_list->count--;

   // Fix the mailbox's slot list pointers if necessary
   // You may need to adjust head_slot and tail_slot pointers if the slot being deallocated was the head or tail of the list
   if (slot == mbox->slot_list->head_slot)
   {                                                // if slot was the head,
      mbox->slot_list->head_slot = slot->next_slot; // new head is slot's next
   }
   if (slot == mbox->slot_list->tail_slot)
   {                                                // if slot was the tail,
      mbox->slot_list->tail_slot = slot->prev_slot; // new tail is the slot's prev
   }
   if (slot->prev_slot != NULL)
   {                                                // if slot had a prev,
      slot->prev_slot->next_slot = slot->next_slot; // prev's next is slot's next
   }
   if (slot->next_slot != NULL)
   {                                                // if slot had a next,
      slot->next_slot->prev_slot = slot->prev_slot; // next's prev is slot's prev
   }

   // clean the next / prev
   slot->next_slot = NULL;
   slot->prev_slot = NULL;

   // Decrement the mailbox's count of available messages
   mbox->available_messages--;

   // Decrement global number of slots 
   numSlot--;
}
