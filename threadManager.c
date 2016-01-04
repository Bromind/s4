#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include "error.h"
#include "threadManager.h"


int requestThreadSafe(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager);

void start_routine_wrapper(struct start_routine_args* args);

void launcher(struct threadManager *manager);

void initManager(struct threadManager *manager)
{
	initLock(&manager->lock);
}

int requestThread(pthread_t *thread, pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager)
{
	int toReturn=0;


	acquire(&manager->lock);
	while(manager->running >= THREAD_LIMIT)
	{
		sched_yield();
	}
	__atomic_fetch_add(&manager->running, 1, __ATOMIC_SEQ_CST);
	release(&manager->lock);


	struct start_routine_args *starter_args = malloc(sizeof(struct start_routine_args));
	if(starter_args == NULL)
	{
		fprintf(stderr, "Can't allocate memory : %s", strerror(errno));
		return EALLOC;
	}
	starter_args->routine = start_routine;
	starter_args->routine_args = args;
	starter_args->counter = &manager->running;

	
	pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

	toReturn = pthread_create(thread, attr, (void*(*)(void*))start_routine_wrapper, starter_args);
	if(toReturn != 0)
	{
		fprintf(stderr, "pthread_create() failed : %s", strerror(toReturn));
		__atomic_fetch_sub(&manager->running, 1, __ATOMIC_SEQ_CST);
	} 
	return toReturn;

}


void start_routine_wrapper(struct start_routine_args* args)
{
	args->routine(args->routine_args);
	__atomic_fetch_sub(args->counter, 1, __ATOMIC_SEQ_CST);
	free(args);
}
