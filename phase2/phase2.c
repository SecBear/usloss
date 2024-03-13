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
int start1 (char *);
extern int start2 (char *);
void check_kernel_mode(char string[]);
void disableInterrupts();
static void enableInterrupts();
int MboxCreate(int slots, int slot_size);
int MboxSend(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondSend();
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondReceive();
int AddToWaitList(mbox_id);
int GetNextMboxID();
char* GetNextReadyMsg(int mbox_id);
void SlotListInit(mail_box *mbox, int slots, int slot_size);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* array of 2000 mail boxes */
mail_box MailBoxTable[MAXMBOX];

/* array of mail box processes (proc table) */
//static struct mbox_proc MboxProcs[MAXSLOTS]; // NOTE: use `i = getpid()%MAXPROC` to get the next pid

int slot_count = 0; // Integer to keep track of total number of slots

unsigned int next_mbox_id = 1;   // The next mbox_id to be assigned
int numMbox = 0;                 // Number of currently active mailboxes


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

   for (int i = 0; i < MAXMBOX; i++)
   {
      // Status, ID and available messages
      MailBoxTable[i].mbox_id = STATUS_UNUSED;
      MailBoxTable[i].status = STATUS_EMPTY;
      MailBoxTable[i].available_messages = STATUS_EMPTY;
      // Slot List
      //MailBoxTable[i].slot_list->mbox_id = STATUS_UNUSED;   // Slot list's MBOX ID
      //MailBoxTable[i].slot_list->count = STATUS_EMPTY;      // Slot list's Slot count
      //MailBoxTable[i].slot_list->head_slot = NULL;          // Slot list's pHead
      //MailBoxTable[i].slot_list->tail_slot = NULL;          // Slot list's pTail
      // Waiting List
      //MailBoxTable[i].waiting_list->count = STATUS_EMPTY;   // Waiting list's count
      //MailBoxTable[i].waiting_list->next_ptr = NULL;        // Waiting list's next pointer
      //MailBoxTable[i].waiting_list->pid = STATUS_UNUSED;    // Waiting list item's pid
   }

   /* Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */

   enableInterrupts();

   /* Create a process for start2, then block on a join until start2 quits */
   if (DEBUG2 && debugflag2)
      console("start1(): fork'ing start2 process\n");
   kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
   if ( join(&status) != kid_pid ) {
      console("start2(): join returned something other than start2's pid\n");
   }

   return 0;
} /* start1 */


void check_kernel_mode(char string[])
{
   int currentPsr =  psr_get();

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
   psr_set( psr_get() & ~PSR_CURRENT_INT );

} /* disableInterrupts */

static void enableInterrupts()  
{
   int currentPsr = psr_get();   // Get current psr
   int interruptEnable = currentPsr | PSR_CURRENT_INT;   // Set the interrupt enable bit to ON (0x2)
   psr_set(interruptEnable);     // Set psr to new psr
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
   if (slots < 1 || slot_size < 1 || slots + slot_count > MAXSLOTS) // If we're trying to create too many slots
   {
      return -1;
   }

   // Similar to what we did with proc table
   // MAXMBOX constant = max number of mailboxes

   // Look through all mailboxes, when found one, return it, 
   // mbox_id % MAXMBOX to wrap around
   mbox_id = GetNextMboxID();

   // Define mailbox in the MailBoxTable
   //struct mailbox mbox = MailBoxTable[mbox_id]; 

   // Single slot mailbox for mutex (only one message in mailbox at a time)
   // multi-slot mailboxes can be used to implement semaphore (>=0)

   SlotListInit(&MailBoxTable[mbox_id], slots, slot_size);  // Initialize slots & slot list
   WaitingListInit(&MailBoxTable[mbox_id]);                 // Initialize waiting list

   MailBoxTable[mbox_id].mbox_id = mbox_id;        // Update mailbox ID
   MailBoxTable[mbox_id].status = STATUS_USED;     // Update mailbox status
   numMbox++;                    // Increment number of mailboxes

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
   check_kernel_mode("MboxSend\n");

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || msg_ptr == NULL)
   {
      // Error message here
      return -1;
   }

   // Get the mailbox from the mail box table
   mail_box mbox = MailBoxTable[mbox_id];

   // If slot is available in this mailbox, allocate a slot from your mail slot table (MboxProcs)
      // Iterate through each item in the mailbox slot list
   slot_ptr current = mbox.slot_list->head_slot;   // Assign current to the head slot (from the mailbox's slot list)
   slot_ptr available_slot = NULL;                 
   while (current != NULL) // While slot exists,
   {
      // Iterate throught the slot list to find an available slot
      if (current->status == STATUS_EMPTY)   // If this slot is empty:
      {
         available_slot = current;           // Assign this slot to current
         break;
      }
      current = current->next_slot;          // Else, check next slot
   }
   if (available_slot == NULL)   // If no available slot was found,
   {
      // block the sender, add sender to waiting list
      // return -1;
      block_me(1); // Not sure what status to use
   }

   // Is anyone waiting? (check waiting list and wake up the first process to start waiting)
      // if so, we need to copy that data into the mailbox and unblock the process that's waiting

   // Block calling process until message is placed in a slot in the mailbox

   // Copy the message into the next empty mail slot
   memcpy(available_slot->message, msg_ptr, msg_size); // Using memcpy instead of strcpy in the case of message not being null-terminated


   // Update slot status and any waiting processes 


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
    // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || msg_ptr == NULL)
   {
      // Error message here
      return -1;
   }

   // Get the mailbox
   mail_box mbox = MailBoxTable[mbox_id];

   // is somebody already waiitng on a send? (block until it's my turn?)
   if (mbox.waiting_list->count > 0)
   {
      block_me(1);
   }

   // block until message is here (using semaphores)
   if (mbox.available_messages <= 0) // Do i have any messages in this mailbox?
   {
      // if no, block_me(), NOTE: MboxSend should unblock this
      block_me(1); // Not sure what the 1 status is or what status we should use here
      // Add to Waiting list of processes to recieve a message?
   }

   // Grab the next available message and free the mailbox slot
   char* message = GetNextReadyMsg(mbox_id);

   // Put the message from the mailbox slot into the receiver's buffer
   memcpy(msg_ptr, message, msg_size);      // Copy the message including null terminator

   // Clean the slot
   

   // disable/enable interrupts?

   return 1; // success
} /* MboxReceive */

int MboxCondReceive(); // non-blocking receive


int check_io(){
    return 0; 
}

// Get the next ready mailbox ID and return it
int GetNextMboxID()
{
   int new_mbox_id = -1; // Initialize new mbox id to -1
   int mboxSlot = next_mbox_id % MAXMBOX;     // Assign new mailbox to next_mbox_id mod MAXMBOX (to wrap around to 1, 2, 3, etc. from max)

   if (numMbox < MAXMBOX)  // If there's room for another process
   {
      // Loop through until we find an empty slot
      while (MailBoxTable[mboxSlot].status != STATUS_EMPTY && mboxSlot != next_mbox_id)
      {
         next_mbox_id++;
         mboxSlot = next_mbox_id % MAXMBOX;
      }

      if (MailBoxTable[mboxSlot].status == STATUS_EMPTY)
      {
         new_mbox_id = next_mbox_id;   // Assigns new_mbox_id to current next_mbox_id value
         next_mbox_id = (next_mbox_id + 1) % MAXMBOX; // Increment next_mbox_id for the next search
      }
   }

   return new_mbox_id;
}

// AddToList functions to add an item to the end of a linked list

// Add the current process to a mailbox's list of watiing processes
int AddToWaitList(mbox_id)
{
   mail_box mbox = MailBoxTable[mbox_id]; // Get mailbox
   int pid = getpid();  // Get process id - not sure how to access processes yet
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
         if (current->pid == pid)
         {
            // Process is already in the list
            return 0;
         }
         current = current->pNext;   // Move to next process
      }

      // Allocate space for new waiting process
      waiting_proc_ptr waiting_process = (waiting_proc_ptr)malloc(sizeof(waiting_proc_ptr)); // Allocate memory for the slot

      // Update new process's pointers
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
waiting_proc_ptr popWaitList(int mbox_id)
{
   // Get waiting list
   waiting_list list = MailBoxTable[mbox_id].waiting_list;

   // Check if list is empty
   if (list->count == 0)
   {
      return NULL;
   }
   
   // Get the oldest item and replace list's head
   waiting_proc_ptr poppedProc = list->pHead;  // get the head of the list (oldest item)
   list->pHead = poppedProc->pNext; // update the head to the next process

   // Update head/tail pointers
   if (list->pHead == NULL)
   {
      list->pTail = NULL;  // If the head becomes NULL (no more items), update the tail as well
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

   return poppedProc;
}

// Get the next ready message in a mailbox
char* GetNextReadyMsg(int mbox_id)
{
   mail_box mbox = MailBoxTable[mbox_id]; // Get the mail box
   char* message = NULL;

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
      if (current->message != NULL)
      {
         // if there is, return it
         return current->message;                              // return the message
      }
      current = current->next_slot; // If not, on to the next slot
   }

   printf("ERROR: GetNextReadyMsg: no slot available?? please investigate\n");
   halt(1);
}

// Initializes the slot list of a mailbox
   // Takes a pointer to the mailbox, the number and size of slots
void SlotListInit(mail_box *mbox, int slots, int slot_size)
{
   int mbox_id;

   // Allocate memory for slot_list
   mbox->slot_list = (struct slot_list*)malloc(sizeof(struct slot_list));
   memset(mbox->slot_list, 0, sizeof(slot_list));   // Initialize the slot list with 0
   mbox->slot_list->head_slot = NULL;
   mbox->slot_list->tail_slot = NULL;
   mbox->slot_list->count = 0;
   mbox->slot_list->mbox_id = mbox_id;

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

      mbox_slot->mbox_id = mbox_id;  // Assign slot's mbox id
      mbox_slot->slot_id = i + 1;        // Assign slot's slot_id
      mbox_slot->status = STATUS_EMPTY;    // Assign slot's status 

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
   } 
}

// Initialize mailbox's waiting list
void WaitingListInit(mail_box *mbox)
{
   int mbox_id;

   // Allocate memory for waiting_list
   mbox->waiting_list = (struct waiting_list*)malloc(sizeof(struct waiting_list));

   // Initialize values
   memset(mbox->waiting_list, 0, sizeof(struct waiting_list));   // Initialize the waiting list with 0s
   mbox->waiting_list->pHead = NULL;
   mbox->waiting_list->pTail = NULL;
   mbox->waiting_list->count = 0;
   mbox->waiting_list->mbox_id = mbox_id;
}
