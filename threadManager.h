#include <pthread.h>
#include "linkedList.h"
#include "ticketLock.h"

#ifndef THREAD_MANAGER
#define THREAD_MANAGER

#ifndef THREAD_LIMIT 
#define THREAD_LIMIT 200
#endif

struct threadManager {
	struct ticket_lock lock;
	volatile size_t running;
};

struct start_routine_args {
	void* routine_args;
	void* (*routine)(void*);
	volatile size_t* counter;
};

void initManager(struct threadManager *manager);

int requestThread(pthread_t* thread, pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager);

#endif
