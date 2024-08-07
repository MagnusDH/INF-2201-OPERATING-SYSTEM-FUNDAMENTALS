/*
 * Implementation of spinlocks, locks and condition variables
 */

#ifndef THREAD_H
#define THREAD_H

#include "kernel.h"

enum
{
	UNLOCKED,
	LOCKED
};

typedef struct {
	pcb_t *waiting; /* waiting queue */
	int status;     /* locked/ unlocked */
} lock_t;

typedef struct {
	pcb_t *condition_queue;	//A queue for waiting threads
} condition_t;

typedef struct {
	int counter;			//Used to check if how many spaces are in the sempahore
	pcb_t *queue;			//Used for pcb's waiting to access semaphore
} semaphore_t;

/* Barrier struct, for a simple shared variable barrier */
typedef struct {
	int max_threads;		//How many threads that are allowed in the barrier
	int thread_counter; 	//Used to count how many PCB's are currently in the barrier
	pcb_t *barrier_queue;	//Used to keep PCB's away from ready_queue
} barrier_t;

/* Lock functions */
void lock_init(lock_t *);
void lock_acquire(lock_t *);
void lock_release(lock_t *);

/* Condition variables */
/* Initialize c */
void condition_init(condition_t *c);
/* Unlock m and block on condition c, when unblocked acquire lock m */
void condition_wait(lock_t *m, condition_t *c);
/* Unblock first thread enqued on c */
void condition_signal(condition_t *c);
/* Unblock all threads enqued on c */
void condition_broadcast(condition_t *c);

/* Semaphore functions */
void semaphore_init(semaphore_t *s, int value);
void semaphore_up(semaphore_t *s);
void semaphore_down(semaphore_t *s);

/*
 * Initialize a barrier, to block all processes entering the barrier
 * until N processes have entered
 */
void barrier_init(barrier_t *b, int n);
/*
 * Make calling process wait at barrier until all N processes have
 * called barrier_wait()
 */
void barrier_wait(barrier_t *b);

#endif /* !THREAD_H */
