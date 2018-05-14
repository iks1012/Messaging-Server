
#include "thread_counter.h"
#include "debug.h" 

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>


typedef struct thread_counter {
	sem_t mutex;
	sem_t zero_mutex;
	volatile int cnt;
} THREAD_COUNTER;


/*
 *
 * Initialize a new thread counter.
 */
THREAD_COUNTER* tcnt_init(){
	THREAD_COUNTER* temp = (THREAD_COUNTER*) malloc(sizeof(THREAD_COUNTER));
	sem_init(&temp->mutex, 0, 1);
	sem_init(&temp->zero_mutex, 0, 1);
	sem_wait(&temp->mutex);
	temp->cnt = 0;
	sem_post(&temp->mutex);
	return temp;
}

 /*
 * Increment a thread counter.
 */
void tcnt_incr(THREAD_COUNTER *tc){
	sem_wait(&tc->mutex);

	int count = tc->cnt;
	if(count == 0){
		sem_wait(&tc->zero_mutex);
	}
	tc->cnt += 1;
	debug("Thread Added: {%d -> %d}", count, count+1);
	sem_post(&tc->mutex);
}


/*
 * Decrement a thread counter, alerting anybody waiting
 * if the thread count has dropped to zero.
 */
void tcnt_decr(THREAD_COUNTER *tc){
	sem_wait(&tc->mutex);
	int count = tc->cnt;
	tc->cnt -= 1;
	debug("Thread removed: {%d -> %d}", count, count-1);
	if(tc->cnt == 0){
		sem_post(&tc->zero_mutex);
		debug("No More Active Threads!");
	}
	sem_post(&tc->mutex);
}



/*
 * Finalize a thread counter.
 */
void tcnt_fini(THREAD_COUNTER *tc){
	sem_destroy(&tc->mutex);
	free(&tc->mutex);
	free(tc);
}




/*
 * A thread calling this function will block in the call until
 * the thread count has reached zero, at which point the
 * function will return.
 */
void tcnt_wait_for_zero(THREAD_COUNTER *tc){
	sem_wait(&tc->zero_mutex);
	sem_post(&tc->zero_mutex);
	return;
}















