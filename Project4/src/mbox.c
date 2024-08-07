/*
 * Implementation of the mailbox.
 * Implementation notes:
 *
 * The mailbox is protected with a lock to make sure that only
 * one process is within the queue at any time.
 *
 * It also uses condition variables to signal that more space or
 * more messages are available.
 * In other words, this code can be seen as an example of implementing a
 * producer-consumer problem with a monitor and condition variables.
 *
 * Note that this implementation only allows keys from 0 to 4
 * (key >= 0 and key < MAX_Q).
 *
 * The buffer is a circular array.
 */

#include "common.h"
#include "mbox.h"
#include "thread.h"
#include "util.h"

mbox_t Q[MAX_MBOX];

/*
 * Returns the number of bytes available in the queue
 * Note: Mailboxes with count=0 messages should have head=tail, which
 * means that we return BUFFER_SIZE bytes.
 */
static int space_available(mbox_t *q) 
{
	if ((q->tail == q->head) && (q->count != 0)) {
		/* Message in the queue, but no space  */
		return 0;
	}

	if (q->tail > q->head) {
		/* Head has wrapped around  */
		return q->tail - q->head;
	}
	/* Head has a higher index than tail  */
	return q->tail + BUFFER_SIZE - q->head;
}


/*
Initialize mailbox system, called by kernel on startup */
void mbox_init(void) 
{	
	for(int i=0; i<MAX_MBOX; i++){
		Q[i].used = 0;						//Number of processes that has opened this mailbox
		lock_init(&Q[i].l);					//Initialize mailbox->lock
		condition_init(&Q[i].moreSpace);	//Initialize mailbox->condition variable
		condition_init(&Q[i].moreData);		//Initialize mailbox->condition variable
		Q[i].count = 0;						//Number of messages in this mailbox
		Q[i].head = 0;						//Points to the first free byte in the buffer
		Q[i].tail = 0;						//Points to the oldest message in the buffer (this is the first to be recieved)
	}
}

/* Open a mailbox with the key 'key'. Returns a mailbox handle which
 * must be used to identify this mailbox in the following functions
 * (parameter q).
 */
int mbox_open(int key) 
{
	if(key > 4 || key < 0){
		ASSERT("Only open a mailbox with key 0-4");
	}

	Q[key].used++;	//Increment number of processes that have opened this mailbox
	return key;		//Return key to identify this mailbox
}

/*Close the mailbox with handle q  */
int mbox_close(int q) 
{
	//Decrement number of processes that have opened this mailbox
	Q[q].used--;

	//Reset mailbox if no other is using it
	if(Q[q].used < 1){
		Q[q].used = 0;
		Q[q].l.status = UNLOCKED;
		Q[q].count = 0;
		Q[q].head = 0;
		Q[q].tail = 0;
	}

	//Return q that identifies this mailbox
	return q;
}

/* Get number of messages (count) and number of bytes available in the
 * mailbox buffer (space). Note that the buffer is also used for
 * storing the message headers, which means that a message will take
 * MSG_T_HEADER + m->size bytes in the buffer. (MSG_T_HEADER =
 * sizeof(msg_t header))
*/
int mbox_stat(int q, int *count, int *space) 
{
	*count = Q[q].count;				//Store the number of messages from mailbox[q] into *count
	*space = space_available(&Q[q]);	//Store the number of bytes available (space) from q into *space
	
	return 1;							//Return 1 OK
}

/*Fetch a message from queue 'q' and store it in 'm'  */
int mbox_recv(int q, msg_t *m) 
{	
	//Aquire lock on q->mailbox, so that no other PCB can access this mailbox simultaneously
	lock_acquire(&Q[q].l);
		
	//If there are NO messages in mailbox to be recieved, loop until one arrives 		
	while(Q[q].count == 0){
		condition_wait(&Q[q].l, &Q[q].moreData);
	}
	
	//From here there is a message in the mailbox

	//Convert message size in mailbox into integer and place it in m->size
	m->size = atoi(Q[q].buffer + Q[q].tail);
	Q[q].tail += strlen(Q[q].buffer + Q[q].tail) +1;	//Including "/0"

	//Copy message from mailbox to m 
	for(int i=0; i<m->size; i++){
		m->body[i] = Q[q].buffer[Q[q].tail];
		Q[q].tail++;

		if(Q[q].tail >= BUFFER_SIZE){
			Q[q].tail = 0;
		}
	}

	//Now there is one less message in mailbox
	Q[q].count--;

	//Release processes waiting for the mailbox to have more available space
	condition_broadcast(&Q[q].moreSpace);

	lock_release(&Q[q].l);

	return 1;
}

/*Insert 'm' into the mailbox 'q'  */
int mbox_send(int q, msg_t *m)
{
	//Aquire lock on q->mailbox, so that no other PCB can access this mailbox simultaneously
	lock_acquire(&Q[q].l);

	//If there is NO space in mailbox for message, loop until there is
	if(space_available(&Q[q]) < (int)MSG_SIZE(m)){
		while(space_available(&Q[q]) < (int)MSG_SIZE(m)){
			condition_wait(&Q[q].l, &Q[q].moreSpace);
		}
	}

	//From here on there is enough space in mailbox for message
	
	//Converting m->size to ASCII string and adding it to mailbox->buffer
	itoa(m->size, Q[q].buffer+Q[q].head);
	Q[q].head += strlen(Q[q].buffer + Q[q].head) +1;	//Including "/0"


	//Copy message from m to mailbox->buffer
	for(int i=0; i<m->size; i++){
		Q[q].buffer[Q[q].head] = m->body[i];
		Q[q].head++;

		if(Q[q].head >= BUFFER_SIZE){
			Q[q].head = 0;
		}
	}

	//Now there is one more message in mailbox
	Q[q].count++;

	//Release processes waiting for a message to arrive in the mailbox
	condition_broadcast(&Q[q].moreData);

	lock_release(&Q[q].l);

	return 1;
}