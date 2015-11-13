#include "coB.h"
#include "color.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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
	/*broadcast->waiting = NULL;*/
	broadcast->nbWaitingThread = 0;
	broadcast->id = id;

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

coBError coBSend(char *message, struct coB *broadcast)
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
	/* TODO : atomic modification of causal tree*/
	node->predecessor = broadcast->causalTree;
	node->predecessor->successor = node;
	broadcast->causalTree = node;
	/* End of atomic modification of causal tree */
	/* Launch the sending (causal dependancy is set now) */

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
	args->message = message;
	args->broadcast = broadcast;
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
	char *message = args->message;
	struct coB *broadcast = args->broadcast;
	struct coMessage *mess = malloc(sizeof(struct coMessage));
	if(mess == NULL)
	{
		fprintf(stderr, "Allocation error");
		return EALLOC;
	}
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
		p2pSend(p2p, i, mess, sizeof(struct coMessage));
	}
	return SUCCESS;
}


void p2pcallback(struct p2pMessage *args)
{
	struct coMessage *message = (struct coMessage*) args->message;
	struct coB *broadcast = (struct coB*) args->upperLayerArgs;
#ifdef LOGGER
	fprintf(stderr, ANSI_GREEN "[coB %i]\tChecking for predecessor\n" ANSI_RESET, broadcast->id);
#endif
	while(! isReceived(message->pred_sender, message->pred_ssn, broadcast))
	{
		/* TODO : Ask for new sending */
		fprintf(stderr, ANSI_RED "[coB %i]\t predecessor not found, aborting\n" ANSI_RESET, broadcast->id);
		return;
	}
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
	/* TODO : atomic modification of causal tree */
	node->predecessor = broadcast->causalTree;
	node->predecessor->successor = node;
	broadcast->causalTree = node;
	/* End of atomic modification of causal tree */
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
		fprintf(stderr,  ANSI_YELLOW "[coB %i]\tWaiting for a message to deliver\n" ANSI_RESET, broadcast->id);
#endif
		while(broadcast->delivered == broadcast->causalTree);
		nextToDeliver = broadcast->delivered->successor;
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
