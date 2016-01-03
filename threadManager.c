#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error.h"
#include "threadManager.h"


struct start_routine_args {
	void* (*routine)(void*);
	void* args;
	size_t* counter;
};

int requestThreadSafe(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager);

void start_routine(struct start_routine_args* args);

void initManager(struct threadManager *manager)
{
	manager->waitingList = newList();
	initLock(&manager->lock);
}

int requestThread(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager)
{
	int toReturn;
	if(manager->running >= THREAD_LIMIT)
	{
		toReturn = requestThreadSafe(thread, attr, start_routine, args, manager);
	} else {
		toReturn = pthread_create(thread, attr, start_routine, args);
		if(toReturn != 0)
		{
			toReturn = requestThreadSafe(thread, attr, start_routine, args, manager);
		} else {
			__atomic_fetch_add(&manager->running, 1, __ATOMIC_SEQ_CST);
		}
	}
	return toReturn;
}


int requestThreadSafe(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager)
{
	struct waitingThread* waitingThread = malloc(sizeof(waitingThread));
	if(waitingThread == NULL)
	{
		fprintf(stderr, "Can't allocate memory : %s", strerror(errno));
		return EALLOC;
	}
	struct start_routine_args *starter_args = malloc(sizeof(struct start_routine_args));
	starter_args->routine = start_routine;
	starter_args->args = args;
	starter_args->counter = &manager->running;
	waitingThread->thread = *thread;
	memcpy((pthread_attr_t*)&waitingThread->attr, attr, sizeof(pthread_t));
	waitingThread->start_routine = start_routine;
	waitingThread->args = starter_args;

	acquire(&manager->lock);
	insertAtEnd(manager->waitingList, waitingThread);
	release(&manager->lock);
	return SUCCESS;
}

void launcher(struct threadManager *manager)
{
	while(manager->running >= THREAD_LIMIT);
	if(! isEmpty(manager->waitingList))
	{
		struct cell * cell = getIndex(manager->waitingList, 0);
		acquire(&manager->lock);
		struct waitingThread* thread = (struct waitingThread*) cell->element;
		removeCell(manager->waitingList, cell);
		release(&manager->lock);
		int res = pthread_create(&thread->thread, &thread->attr, thread->start_routine, thread->args);
		if(res != 0)
		{
			fprintf(stderr, "pthread_create() failed : %s", strerror(res));
		} else {
			__atomic_fetch_add(&manager->running, 1, __ATOMIC_SEQ_CST);
		}
	}
	
}

void start_routine(struct start_routine_args* args)
{
	args->routine(args->args);
	__atomic_fetch_sub(args->counter, 1, __ATOMIC_SEQ_CST);
}
