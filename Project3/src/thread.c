/*
 * Implementation of locks and condition variables
 */

#include "common.h"
#include "interrupt.h"
#include "scheduler.h"
#include "thread.h"
#include "util.h"

void lock_init(lock_t *l) {
	/*
	 * no need for critical section, it is callers responsibility to
	 * make sure that locks are initialized only once
	 */
	l->status = UNLOCKED;
	l->waiting = NULL;		//l->waiting is a pcb_t
}

/* Acquire lock without critical section (called within critical section) */
static void lock_acquire_helper(lock_t *l)
{
	//If lock is NOT taken
	if(l->status == UNLOCKED){
		l->status = LOCKED;
	}
	//If lock is taken, block current_running
	else{
		block(&l->waiting);
		l->status = LOCKED;
	}
}

void lock_acquire(lock_t *l)
{
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();

	lock_acquire_helper(l);
	
	leave_critical();
}

void lock_release(lock_t *l) 
{
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();
	
	l->status = UNLOCKED;
	
	//Release all threads waiting in lock->queue
	if(l->waiting != NULL){
		unblock(&l->waiting);
	}

	leave_critical();
}


/* CONDITION FUNCTIONS */

/*Initializes a condition variable*/
void condition_init(condition_t *c) {
	c->condition_queue = NULL;
}

/*
 * unlock m and block the thread (enqued on c), when unblocked acquire
 * lock m
 */

/*Releases given lock and places current_running into condition->queue, preventing it from running
  When released it will acquire lock again*/
void condition_wait(lock_t *m, condition_t *c)
{
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();
	
	//Release lock for other to access it, while thread waits
	lock_release(m);

	//Make current_running wait by placing it in conditional_queue 
	block(&c->condition_queue);

	lock_acquire_helper(m);

	leave_critical();
}

/* unblock first thread enqued on c */
void condition_signal(condition_t *c)
{
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();
	
	if(c->condition_queue != NULL){
		unblock(&c->condition_queue);
	}

	leave_critical();	
}

/* unblock all threads enqued on c */
void condition_broadcast(condition_t *c)
{
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();
	
	while(c->condition_queue != NULL){
		unblock(&c->condition_queue);
	}

	leave_critical();
}


/* SEMAPHORE FUNCTIONS */

/*Initializes a semaphore*/
void semaphore_init(semaphore_t *s, int value) {
	s->counter = value;	//Initializing that this semaphore is available
	s->queue = NULL;	//Initializing that the semaphore->queue is empty
}

//Semaphore WAIT
void semaphore_down(semaphore_t *s) {
	
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();

	//If semaphore is available, do not block thread. Set semaphore to have one less space
	if(s->counter > 0){
		s->counter--;
		//Continue execution
	}
	//If no space in semaphore, place current_running in semaphore_queue and wait for semaphore to become available
	else if(s->counter <= 0){
		block(&s->queue);
	}

	leave_critical();
}

/*Equal to "Semaphore SIGNAL"*/
void semaphore_up(semaphore_t *s) {

	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();

	//If there are no threads waiting in semaphore->queue, increment the counter to signal there is space in the semaphore
	if(s->queue == NULL){
		s->counter++;
	}
	
	//If there are threads waiting in semaphore->queue, unblock ONE thread
	//the semaphore will still be 0, but there will be one fewer process sleeping on it
	else if(s->queue != NULL){
		// s->counter++;
		unblock(&s->queue);
	}

	leave_critical();
}


/*
 * Barrier functions
 * Note that to test these functions you must set NUM_THREADS
 * (kernel.h) to 9 and uncomment the start_addr lines in kernel.c.
 */

/* n = number of threads that waits at the barrier */
void barrier_init(barrier_t *b, int n)
{
	b->max_threads = n;
	b->thread_counter = 0;
	b->barrier_queue = NULL;
}

/* Wait at barrier until all n threads reach it */
void barrier_wait(barrier_t *b)
{
	//Enter critical section so that multiple threads dont mess with this section simultaneously
	enter_critical();
	b->thread_counter++;

	//If barrier has space, place current_running in barrier->queue
	if(b->thread_counter < b->max_threads){
		block(&b->barrier_queue);
	}
	//If max threads in barrier have been reached, release all threads back into ready_queue
	else if(b->thread_counter == b->max_threads){
		//Release threads from barrier_queue into ready_queue
		for(int i=0; i<b->thread_counter-1; i++){
			unblock(&b->barrier_queue);
		}

		b->thread_counter = 0;
	}

	leave_critical();
}