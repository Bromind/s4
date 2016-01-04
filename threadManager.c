#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error.h"
#include "threadManager.h"


int requestThreadSafe(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager);

void start_routine_wrapper(struct start_routine_args* args);

void launcher(struct threadManager *manager);

void initManager(struct threadManager *manager)
{
	manager->waitingList = newList();
	initLock(&manager->lock);
	pthread_t launcher_thread;
	pthread_attr_t launcher_attr;
	if(pthread_attr_init(&launcher_attr) != 0)
	{
		fprintf(stderr, "pthread_attr_init() failed\n");
	} else {
		int res = pthread_create(&launcher_thread, &launcher_attr,(void*(*)(void*)) &launcher, manager);
		if(res != 0)
		{
			fprintf(stderr, "pthread_create() failed : %s\n", strerror(res));
		}
	}
}

int requestThread(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager)
{
	int toReturn;
	if(manager->running >= THREAD_LIMIT)
	{
		printf("Requesting Thread (limit reached (%zu))\n", manager->running);
		toReturn = requestThreadSafe(thread, attr, start_routine, args, manager);
	} else {
		printf("Auto-launch\n");
		__atomic_fetch_add(&manager->running, 1, __ATOMIC_SEQ_CST);
		struct start_routine_args *starter_args = malloc(sizeof(struct start_routine_args));
		if(starter_args == NULL)
		{
			fprintf(stderr, "Can't allocate memory : %s", strerror(errno));
			return EALLOC;
		}
		starter_args->routine = start_routine;
		starter_args->args = args;
		starter_args->counter = &manager->running;
		toReturn = pthread_create(thread, attr, (void*(*)(void*))start_routine_wrapper, starter_args);
		if(toReturn != 0)
		{
			printf("Requesting Thread (failed to launch)\n");
			toReturn = requestThreadSafe(thread, attr, start_routine, args, manager);
			__atomic_fetch_sub(&manager->running, 1, __ATOMIC_SEQ_CST);
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
	if(starter_args == NULL)
	{
		fprintf(stderr, "Can't allocate memory : %s", strerror(errno));
		return EALLOC;
	}
	starter_args->routine = start_routine;
	starter_args->args = args;
	starter_args->counter = &manager->running;
	waitingThread->thread = *thread;
	memcpy((pthread_attr_t*)&waitingThread->attr, attr, sizeof(pthread_t));
	waitingThread->start_routine_wrapper = start_routine_wrapper;
	waitingThread->args = starter_args;

	acquire(&manager->lock);
	insertAtEnd(manager->waitingList, waitingThread);
	int listSize = size(manager->waitingList);
	release(&manager->lock);
	printf("Registered thread (%i)\n", listSize);
	return SUCCESS;
}

void launcher(struct threadManager *manager)
{
	while(manager->running >= THREAD_LIMIT);
		printf("Thread limit reached (%zu)\n", manager->running);
	printf("dequeueing thread\n");
	if(! isEmpty(manager->waitingList))
	{
		struct cell * cell = getIndex(manager->waitingList, 0);
		acquire(&manager->lock);
		struct waitingThread* thread = (struct waitingThread*) cell->element;
		removeCell(manager->waitingList, cell);
		release(&manager->lock);
		int res = pthread_create(&thread->thread, &thread->attr, (void*(*)(void*))thread->start_routine_wrapper, thread->args);
		if(res != 0)
		{
			fprintf(stderr, "pthread_create() failed : %s", strerror(res));
		} else {
			__atomic_fetch_add(&manager->running, 1, __ATOMIC_SEQ_CST);
		}
	}
	
}

void start_routine_wrapper(struct start_routine_args* args)
{
	printf("start_routine_wrapper()\n");
	args->routine(args->args);
	free(args);
	size_t running = __atomic_fetch_sub(args->counter, 1, __ATOMIC_SEQ_CST);
	printf("Thread finished (%zu)\n", running);
}
