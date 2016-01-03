#include "coB.h"
#include "color.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>

/*
 * message format : 
 * [id message = sender:ssn]
 * [id predece = pred sender : pred ssn]
 * content
*/

void p2pcallback(struct p2pMessage *args);
int getTimestamp(struct coB *broadcast);
int isReceived(int sender, int ssn, struct coB *broadcast);
coBError coSender(struct coSenderArgs *args);
void delivrer(struct coBDelivrerArgs *args);
void inBandMessage(struct coMessage* message, struct coB* broadcast);
void outBandMessage(struct coMessage* message, struct coB* broadcast);

void wait(unsigned int time);

coBError coBInit(char **addrs, char **ports, size_t nbDest, char *localAddr, 
		char *localPort, void* (*callback)(void*), 
		struct coB *broadcast, int id)
{
	/* 
	 * TODO : 
	 * - Check for twice the same addr:port
	 * - What to do in case of error ?
	 */

	size_t i = 0;
	p2pError res = 0;
	res = init_p2p(localPort, localAddr, &(broadcast->p2p), 
			(void*(*)(void*))&p2pcallback, broadcast);
	if(res != SUCCESS)
	{
		fprintf(stderr, "Error when initializing p2pChannel\n");
		return res;
	}
	for (; i < nbDest ; i++, res = SUCCESS)
	{
		res = init_p2p_sender(ports[i], addrs[i], &(broadcast->p2p));
		if( res != SUCCESS ) 
		{
			fprintf(stderr, "Error when initializing p2pSender\n");
		}
	}
	
	struct coNode *root = NULL;
	root = malloc(sizeof(struct coNode));
	if(root == NULL)
	{
		fprintf(stderr, "Allocation error\n");
		return EALLOC;
	}
	root->sender = -1;
	root->ssn = -1;
	root->predecessor = NULL;
	root->successor = NULL;
	
	broadcast->timestamp = 0;
	broadcast->delivered = root;
	broadcast->causalTree = root;
	broadcast->first = root;
	broadcast->nbReceivers = nbDest;

	/* Ugly, but we have no-choice to initialize id and to make it constant after */
	int* id_modifiable = (int*)&broadcast->id;
	*id_modifiable = id;
	
	initLock(&broadcast->lock);
	broadcast->pause_delivrer = 5000;
	broadcast->pause_request = 5000;

	struct coBDelivrerArgs *delArgs = NULL;
	delArgs = malloc(sizeof(struct coBDelivrerArgs));
	if(delArgs == NULL)
	{
		fprintf(stderr, "Allocation error\n");
		return EALLOC;
	}
	delArgs->callback = callback;
	delArgs->broadcast = broadcast;

	pthread_t thread;
	pthread_attr_t attr;
	if(pthread_attr_init(&attr) != 0)
	{
		fprintf(stderr, "pthread_attr_init() failed\n");
	}
	res = pthread_create(&thread, &attr, (void*(*)(void*))delivrer, delArgs);
	if(res != 0)
	{
		fprintf(stderr, "pthread_create() failed : %i\n", res);
	}
	
	return SUCCESS;
}


coBError closeBroadcaster(struct coB *broadcast)
{

	char flags = OUT_OF_BAND | END_OF_COMMUNICATION;
	
	coBSend(NULL, broadcast, flags);
	
	int i;
	for(i = 0 ; i < broadcast->nbReceivers ; i++)
		delete_p2p_sender(i, &broadcast->p2p);
	struct coNode* tree = broadcast->causalTree;
	while(tree->predecessor != NULL)
	{
		tree = tree->predecessor;
		free(tree->successor);
	}
	free(tree);
	return SUCCESS;
}

coBError coBSend(char *message, struct coB *broadcast, char flags)
{
	struct coNode *node = NULL;
	node = malloc(sizeof(struct coNode));
	if(node == NULL) {
		fprintf(stderr, "Allocation error\n");
		return EALLOC;
	}
	node->sender = broadcast->id;
	if(message != NULL)
		memcpy(&node->buffer, message, CO_BUFF_SIZE);
	/* TODO : atomic modification of causal tree if not out of band (see flags) */
	if((flags & OUT_OF_BAND) == 0)
	{
		node->ssn = getTimestamp(broadcast);
		acquire(&broadcast->lock);
		node->predecessor = broadcast->causalTree;
		node->predecessor->successor = node;
		broadcast->causalTree = node;
		/* End of atomic modification of causal tree */
		/* Launch the sending (causal dependancy is set now) */
		release(&broadcast->lock);
	}

	pthread_t thread;
	pthread_attr_t attr;
	struct coSenderArgs *args;
	if(pthread_attr_init(&attr) != 0)
	{
		fprintf(stderr, "pthread_attr_init() failed\n");
		return EPTHREAD;
	}
	args = malloc(sizeof(struct coSenderArgs));
	memcpy(&args->node, node, sizeof(struct coNode));
	args->broadcast = broadcast;
	args->flags = flags;
	int res = pthread_create(&thread, &attr, (void*(*)(void*))coSender, args);
	if (res != 0) 
	{
		fprintf(stderr, "pthread_create() failed : %i\n", res);
	}
	return res;
}

coBError coSender(struct coSenderArgs *args)
{
	struct coNode node = args->node;
	char *message = node.buffer;
	struct coB *broadcast = args->broadcast;
	char flags = args->flags;
	struct coMessageBuilder *mess = malloc(sizeof(struct coMessageBuilder));
	if(mess == NULL)
	{
		fprintf(stderr, "Allocation error");
		return EALLOC;
	}
	mess->flags = flags;
	mess->sender = node.sender;
	mess->ssn = node.ssn;
	if(node.predecessor != NULL)
	{
		mess->pred_sender = node.predecessor->sender;
		mess->pred_ssn = node.predecessor->ssn;
	} else {
		mess->pred_sender = -1;
		mess->pred_ssn = -1;
	}
	memcpy(mess->buffer, message, CO_BUFF_SIZE);
	
	struct p2pChannel *p2p = &broadcast->p2p;
	int i;
	for(i = 0 ; i < p2p->nb_senders ; i++)
	{
		/* Send to all but ourself (already in our tree)*/
		if(i != broadcast->id)
			p2pSend(p2p, i, (struct coMessage*)mess, sizeof(struct coMessage));
	}
	return SUCCESS;
}


void p2pcallback(struct p2pMessage *args)
{
	struct coMessage *message = (struct coMessage*) args->message;
	struct coB *broadcast = (struct coB*) args->upperLayerArgs;
#ifdef LOGGER
		fprintf(stderr, ANSI_GREEN "[coB %i]\t Receiving message (sender : %i, ssn : %i)\n" ANSI_RESET, broadcast->id, message->sender, message->ssn);
#endif

	if(((message->flags & OUT_OF_BAND) == 0) 
		|| (message->flags == (OUT_OF_BAND | COPY))
	){
#ifdef LOGGER
		fprintf(stderr, ANSI_GREEN "[coB %i]\t Receiving in band message (sender : %i, ssn : %i)\n" ANSI_RESET, broadcast->id, message->sender, message->ssn);
#endif
		inBandMessage(message, broadcast);
	} else {
#ifdef LOGGER
		fprintf(stderr, ANSI_YELLOW "[coB %i]\t Receiving out of band message (sender : %i, ssn : %i)\n" ANSI_RESET, broadcast->id, message->sender, message->ssn);
#endif
		outBandMessage(message, broadcast);
	}
}

void outBandMessage(struct coMessage* message, struct coB* broadcast)
{
	if((message->flags & REQUEST) != 0)
	{
		struct coNode* current = broadcast->causalTree;
		while((current != NULL)
			&& (current->ssn != message->pred_ssn)
		){
			current = current->predecessor;
		}
		if(current == NULL)
		{
#ifdef LOGGER
			fprintf(stderr, ANSI_RED "[coB %i]\t asked to send a copy of ssn %i but it is not found in the tree" ANSI_RESET, broadcast->id, message->pred_ssn);
#endif
			return;
		}
#ifdef LOGGER
		fprintf(stderr, ANSI_BLUE "[coB %i]\t sending a copy of %i\n" ANSI_RESET, broadcast->id, message->pred_ssn);
#endif
		struct coSenderArgs args;
		args.flags = OUT_OF_BAND | COPY;
		args.node = *current;
		args.broadcast = broadcast;
		coSender(&args);
	}
}

void inBandMessage(struct coMessage* message, struct coB* broadcast)
{
	/* Non-safe duplication elimination, at first. Not safe but fast (no lock)*/
	if(isReceived(message->sender, message->ssn, broadcast))
	{
#ifdef LOGGER
	fprintf(stderr, ANSI_MAGENTA "[coB %i]\t Receiving duplicate ssn=%i from sender=%i\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	return;
	}
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Checking for predecessor (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	while(! isReceived(message->pred_sender, message->pred_ssn, broadcast))
	{
		/* Non-safe duplication elimination, at second time, at each round. Not safe but fast (no lock)*/
		if(isReceived(message->sender, message->ssn, broadcast))
		{
#ifdef LOGGER
			fprintf(stderr, ANSI_MAGENTA "[coB %i]\t Receiving duplicate ssn=%i from sender=%i\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
			return;
		}
		fprintf(stderr, ANSI_RED "[coB %i]\t predecessor (ssn : %i, sender : %i) not found, sending request to %i (pause = %i)\n" ANSI_RESET, broadcast->id, message->pred_ssn, message->pred_sender, message->pred_sender, broadcast->pause_request);
		/*
			Request message format : 
			flags = OUT_OF_BAND | REQUEST
			sender = id of requester
			ssn = undefined
			pred_sender = id of requested message sender
			pred_ssn = id of requested message
			buffer = undefined
		*/
		struct coMessageBuilder *request = NULL;
		request = malloc(sizeof(struct coMessageBuilder));
		request->flags = OUT_OF_BAND | REQUEST;
		request->sender = broadcast->id;
		request->pred_sender = message->pred_sender;
		request->pred_ssn = message->pred_ssn;
		int i;
		for(i = 0 ; i < broadcast->p2p.nb_senders ; i++)
		{
			/* Send to all but ourself */
			if(i != broadcast->id)
				p2pSend(&broadcast->p2p, i, (struct coMessage*)request, sizeof(struct coMessage));
		}

		/* Given someone else chance to go, if any*/
		wait(broadcast->pause_request);
		broadcast->pause_request *= 2;
		if(broadcast->pause_request > MAX_WAIT)
			broadcast->pause_request = MAX_WAIT ;
	}
	broadcast->pause_request = broadcast->pause_request/2 + 1;
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Predecessor found (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	struct coNode *node = malloc(sizeof(struct coNode));
	if(node == NULL)
	{
		fprintf(stderr, "Allocation error\n");
		return;
	}
	node->sender = message->sender;
	node->ssn = message->ssn;
	memcpy(&node->buffer, &message->buffer, CO_BUFF_SIZE);
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Waiting for lock (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	/* TODO : atomic modification of causal tree */
	acquire(&broadcast->lock);
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Lock acquired (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	/* Safe check for redundancy in the tree */
	if(!isReceived(message->sender, message->ssn, broadcast))
	{
		node->predecessor = broadcast->causalTree;
		node->predecessor->successor = node;
		broadcast->causalTree = node;
	} else {
#ifdef LOGGER 
		fprintf(stderr, ANSI_MAGENTA "[coB %i]\t (ssn : %i, sender : %i) is already received, removing" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	}
	/* End of atomic modification of causal tree */
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Lock released (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	release(&broadcast->lock);
	free(message);
	return;
		
}

int isReceivedRec(int sender, int ssn, struct coNode *tree)
{
	/* At the root of the tree and not found, i.e. not received yet*/
	if(tree == NULL)
	{
		return 0;
	}
	return (tree->sender == sender && tree->ssn == ssn) || isReceivedRec(sender, ssn, tree->predecessor);
}

int isReceived(int sender, int ssn, struct coB *broadcast)
{
	return isReceivedRec(sender, ssn, broadcast->causalTree);
}

void delivrer(struct coBDelivrerArgs *args)
{
	void*(*callback)(void*) = args->callback;
	struct coB *broadcast = args->broadcast;
	char *message;
	struct coNode *nextToDeliver;
	for(;;)
	{
		while(broadcast->delivered == broadcast->causalTree){
#ifdef LOGGER
		fprintf(stderr,  ANSI_YELLOW "[coB %i]\t Waiting %i for a message to deliver\n" ANSI_RESET, broadcast->id, broadcast->pause_delivrer);
#endif
			wait(broadcast->pause_delivrer);
			broadcast->pause_delivrer += PAUSE_DELIVRER_DEFAULT;
		}
		broadcast->pause_delivrer = PAUSE_DELIVRER_DEFAULT;
		nextToDeliver = broadcast->delivered->successor;
#ifdef LOGGER
		fprintf(stderr, ANSI_GREEN "[coB %i]\t Message (ssn : %i, sender : %i) being delivered\n" ANSI_RESET, broadcast->id, nextToDeliver->ssn, nextToDeliver->sender);
#endif
		message = nextToDeliver->buffer;

//		pthread_t thread;
//		pthread_attr_t attr;
//		if(pthread_attr_init(&attr) != 0)
//		{
//			fprintf(stderr, "pthread_attr_init() failed\n");
//		}
//		int res = pthread_create(&thread, &attr, callback, message);
//		if(res != 0)
//		{
//			fprintf(stderr, "pthread_create() failed : %i\n", res);
//		}
		callback(message);
		broadcast->delivered = nextToDeliver;
	}
}

int getTimestamp(struct coB *broadcast)
{
	/* TODO : T&T&S */
	int tmp = broadcast->timestamp;
	broadcast->timestamp++;
	return tmp;
}

void printTree(struct coB *broadcast)
{
	const struct coNode *current = broadcast->first;
	char *color = ANSI_CYAN;
	for(;current != NULL;current = current->successor)
	{
		fprintf(stdout, "%ssender : %i, ssn : %i, successor = %p, message : %s\n" ANSI_RESET, color, current->sender, current->ssn, current->successor, current->buffer);
		if(current == broadcast->delivered)
		{
			color = ANSI_MAGENTA;
		}
	}	
}

void wait(unsigned int time)
{
	int err = usleep(time);// sched_yield();
	if(err == -1)
	{
		fprintf(stderr, "wait failed : %s", strerror(errno));
		return;
	}
}
