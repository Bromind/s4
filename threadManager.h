#include <pthread.h>
#include "linkedList.h"
#include "ticketLock.h"

#ifndef THREAD_LIMIT 
#define THREAD_LIMIT 40
#endif

struct waitingThread {
	pthread_t thread;
	const pthread_attr_t attr;
	void* (*start_routine)(void*);
	void* args;
};

struct threadManager {
	struct linkedList* waitingList;
	struct ticket_lock lock;
	size_t running;
};

void initManager(struct threadManager *manager);

int requestThread(pthread_t* thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void* args, struct threadManager *manager);
