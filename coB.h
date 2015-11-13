#include "p2p.h"
#include "error.h"

#define CO_BUFF_SIZE 25

typedef int coBError;

struct coCallbackArgs {
	struct coMessage *msg;
	struct coB *broadcast;
};

struct coMessage {
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
};

struct coB {
	struct coNode *causalTree;
	struct coNode *delivered;
	/*struct coNode **waiting;*/
	int timestamp;
	int id;
	size_t nbWaitingThread;
	struct p2pChannel p2p;
};

struct coSenderArgs {
	struct coNode *node;
	char *message;
	struct coB *broadcast;
};

coBError coBInit(char **addrs, char **ports, size_t nbDest, char *localAddr, 
		char *localPort, void* (*callback)(void*), 
		struct coB *broadcast, int id);

coBError coBSend(char *message, struct coB *broadcast);
