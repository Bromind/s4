#include "ticketLock.h"

void acquire(struct ticket_lock* lock)
{
	unsigned int myticket = __atomic_fetch_add(&lock->tail, 1, __ATOMIC_SEQ_CST);
	unsigned int current = lock->head;
	while(current != myticket)
{
	current = lock->head;
	asm volatile ("nop");
}
	asm volatile ("" ::: "memory");
}

void release(struct ticket_lock* lock)
{
	asm volatile ("" ::: "memory");
	__atomic_fetch_add(&lock->head, 1, __ATOMIC_SEQ_CST);
}

void initLock(struct ticket_lock* lock)
{
	lock->head = 0;
	lock->tail = 0;
}
