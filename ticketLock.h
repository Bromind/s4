#ifndef TICKET_LOCK
#define TICKET_LOCK
struct ticket_lock {
	volatile unsigned int head;
	volatile unsigned int tail;
};

void acquire(struct ticket_lock*);
void release(struct ticket_lock*);
void initLock(struct ticket_lock*);
#endif
