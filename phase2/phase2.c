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

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* array of 2000 mail boxes */
mail_box MailBoxTable[MAXMBOX];

/* array of mail box processes (proc table) */
static struct mbox_proc MboxProcs[MAXSLOTS]; // NOTE: use `i = getpid()%MAXPROC` to get the next pid

int slot_count = 0; // Integer to keep track of total number of slots


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
      console("start1(): at beginning\n");

   check_kernel_mode("start1");

   /* Disable interrupts */
   disableInterrupts();

   /* Initialize the mail box table, slots, & other data structures.
    * Initialize int_vec and sys_vec, allocate mailboxes for interrupt
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
   mbox_id = getNextReadyMboxID();

   // Define mailbox in the MailBoxTable
   struct mailbox mbox = MailBoxTable[mbox_id]; 

   // Single slot mailbox for mutex (only one message in mailbox at a time)
   // multi-slot mailboxes can be used to implement semaphore (>=0)

   // Initialize mailbox items
   mbox.mbox_id = mbox_id;
   mbox.slot_list->count = slots;
   mbox.slot_list->head_slot = NULL;
   mbox.slot_list->tail_slot = NULL;

   // Initialize mailbox slots
   slot_ptr prev_slot = NULL;
   for (int i = 0; i < slots; ++i)
   {
      slot_ptr mbox_slot = (slot_ptr)malloc(slot_size); // Allocate memory for the slot
      mbox_slot->mbox_id = mbox_id;  // Assign slot's mbox id
      mbox_slot->status = UNUSED;    // Assign slot's status 

      // Update pointers in the slot list
      if (mbox.slot_list->head_slot == NULL)
      {
         // If this is the first slot in the list
         mbox.slot_list->head_slot = mbox_slot;    // Assign current to head
         mbox.slot_list->tail_slot = mbox_slot;    // Assign current to tail
      }
      else
      {
         // Add the slot to the end of the list
         mbox.slot_list->tail_slot->next_slot = mbox_slot;  // Assign current to previous tail's next
         mbox.slot_list->tail_slot = mbox_slot;             // Assign current to tail
      }  
   } 

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
   check_kernel_mode();

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
   slot_ptr current = mbox.slot_list->head_slot;
   slot_ptr available_slot = NULL;
   while (current != NULL) 
   {
      // Iterate throught the slot list to find an available slot
      if (current->status == UNUSED)
      {
         available_slot = current;
         break;
      }
      current = current->next_slot;
   }
   if (available_slot == NULL)
   {
      // If no available slot was found, block the sender, add sender to waiting list
      //return -1;
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
   // is somebody already waiitng on a send? (not sure what to do in this case)

   // block until message is here (using semaphores)
      // Do i have any messages in this mailbox?
         // if no, block_me(), NOTE: MboxSend should unblock this
   // Add to Waiting list of processes to recieve a message?

   // Pull the message from the mailbox slot to the calling process

   // disable/enable interrupts?

} /* MboxReceive */

int MboxCondReceive(); // non-blocking receive


int check_io(){
    return 0; 
}

// Get the next ready mailbox ID and return it
int GetNextReadyMboxID();
{
   int mbox_id = 0;

   return mbox_id;
}

// AddToList function to add an item to the end of a linked list

// PopList function to pop the first item added to the linked list (head)
