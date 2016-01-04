#include <pthread.h>
#include "linkedList.h"
#include "ticketLock.h"

#ifndef THREAD_MANAGER
#define THREAD_MANAGER

#ifndef THREAD_LIMIT 
#define THREAD_LIMIT 2
#endif

struct threadManager {
	struct linkedList* waitingList;
	struct ticket_lock lock;
	volatile size_t running;
};

struct start_routine_args {
	void* (*routine)(void*);
	void* args;
	volatile size_t* counter;
};

struct waitingThread {
	pthread_t thread;
	const pthread_attr_t attr;
	void (*start_routine_wrapper)(struct start_routine_args*);
	void* args;
};

void initManager(struct threadManager *manager);

int requestThread(pthread_t* thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager);

#endif
