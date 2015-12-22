#include "p2p.h"
#include "error.h"
#include "ticketLock.h"

#define CO_BUFF_SIZE 25
#define OUT_OF_BAND (1 << 0) /* This is not an original copy */
#define REQUEST (1 << 1) /* Please send me a copy of this message. */
#define END_OF_COMMUNICATION (1 << 2) /* These are my last words. */
#define COPY (1 << 3) /* this message is a copy of a previous one. */

typedef int coBError;

struct coCallbackArgs {
	struct coMessage *msg;
	struct coB *broadcast;
};

struct coMessage {
	char flags;
	int sender;
	int ssn;
	int pred_sender;
	int pred_ssn;
	char buffer[CO_BUFF_SIZE];
};

struct coNode {
	int sender;
	int ssn;
	struct coNode *predecessor;
	struct coNode *successor;
	char buffer[CO_BUFF_SIZE];
};

struct coBDelivrerArgs {
	void*(*callback)(void*);
	struct coB *broadcast;
};

struct coB {
	struct coNode *causalTree;
	struct coNode *delivered;
	struct coNode *first;
	/*struct coNode **waiting;*/
	int timestamp;
	int id;
	size_t nbWaitingThread;
	struct p2pChannel p2p;
	struct ticket_lock lock;
	unsigned int pause;
};

struct coSenderArgs {
	char flags;
	struct coNode *node;
	struct coB *broadcast;
};

coBError coBInit(char **addrs, char **ports, size_t nbDest, char *localAddr, 
		char *localPort, void* (*callback)(void*), 
		struct coB *broadcast, int id);

coBError coBSend(char *message, struct coB *broadcast, char flags);

void printTree(struct coB *broadcast);
