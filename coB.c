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
	broadcast->nbWaitingThread = 0;
	broadcast->id = id;
	
	initLock(&broadcast->lock);
	broadcast->pause = 500;

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

coBError coBSend(char *message, struct coB *broadcast, char flags)
{
	struct coNode *node = NULL;
	node = malloc(sizeof(struct coNode));
	if(node == NULL) {
		fprintf(stderr, "Allocation error\n");
		return EALLOC;
	}
	node->sender = broadcast->id;
	node->ssn = getTimestamp(broadcast);
	memcpy(&node->buffer, message, CO_BUFF_SIZE);
	/* TODO : atomic modification of causal tree if not out of band (see flags) */
	if((flags & OUT_OF_BAND) == 0)
	{
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
	args->node = node;
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
	struct coNode *node = args->node;
	char *message = node->buffer;
	struct coB *broadcast = args->broadcast;
	char flags = args->flags;
	struct coMessage *mess = malloc(sizeof(struct coMessage));
	if(mess == NULL)
	{
		fprintf(stderr, "Allocation error");
		return EALLOC;
	}
	mess->flags = flags;
	mess->sender = node->sender;
	mess->ssn = node->ssn;
	if(node->predecessor != NULL)
	{
		mess->pred_sender = node->predecessor->sender;
		mess->pred_ssn = node->predecessor->ssn;
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
			p2pSend(p2p, i, mess, sizeof(struct coMessage));
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
			fprintf(stderr, ANSI_RED "[coB %i]\t asked to send a copy of ssn %i but it is not found in the tree" ANSI_RESET, broadcast->id, message->pred_ssn);
			return;
		}
#ifdef LOGGER
		fprintf(stderr, ANSI_BLUE "[coB %i]\t sending a copy of %i\n" ANSI_RESET, broadcast->id, message->pred_ssn);
#endif
		struct coSenderArgs args;
		args.flags = OUT_OF_BAND | COPY;
		args.node = current;
		args.broadcast = broadcast;
		coSender(&args);
	}
}

void inBandMessage(struct coMessage* message, struct coB* broadcast)
{
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
		fprintf(stderr, ANSI_RED "[coB %i]\t predecessor (ssn : %i, sender : %i) not found, sending request to %i\n" ANSI_RESET, broadcast->id, message->pred_ssn, message->pred_sender, message->pred_sender);
		/*
			Request message format : 
			flags = OUT_OF_BAND | REQUEST
			sender = id of requester
			ssn = undefined
			pred_sender = id of requested message sender
			pred_ssn = id of requested message
			buffer = undefined
		*/
		struct coMessage *request = NULL;
		request = malloc(sizeof(struct coMessage));
		request->flags = OUT_OF_BAND | REQUEST;
		request->sender = broadcast->id;
		request->pred_sender = message->pred_sender;
		request->pred_ssn = message->pred_ssn;
		p2pSend(&broadcast->p2p, request->pred_sender, request, sizeof(struct coMessage));

		/* Given someone else chance to go, if any*/
		wait(broadcast->pause);
	}
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
	free(message);
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Waiting for lock (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	/* TODO : atomic modification of causal tree */
	acquire(&broadcast->lock);
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Lock acquired (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	node->predecessor = broadcast->causalTree;
	node->predecessor->successor = node;
	broadcast->causalTree = node;
	/* End of atomic modification of causal tree */
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\t Lock released (ssn : %i, sender : %i)\n" ANSI_RESET, broadcast->id, message->ssn, message->sender);
#endif
	release(&broadcast->lock);
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
#ifdef LOGGER
		fprintf(stderr,  ANSI_YELLOW "[coB %i]\t Waiting for a message to deliver\n" ANSI_RESET, broadcast->id);
#endif
		while(broadcast->delivered == broadcast->causalTree);
		nextToDeliver = broadcast->delivered->successor;
#ifdef LOGGER
		fprintf(stderr, ANSI_GREEN "[coB %i]\t Message (ssn : %i, sender : %i) being delivered\n" ANSI_RESET, broadcast->id, nextToDeliver->ssn, nextToDeliver->sender);
#endif
		message = nextToDeliver->buffer;

		pthread_t thread;
		pthread_attr_t attr;
		if(pthread_attr_init(&attr) != 0)
		{
			fprintf(stderr, "pthread_attr_init() failed\n");
		}
		int res = pthread_create(&thread, &attr, callback, message);
		if(res != 0)
		{
			fprintf(stderr, "pthread_create() failed : %i\n", res);
		}
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
	struct coNode *current = broadcast->first;
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
