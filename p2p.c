#include "p2p.h"
#include "color.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef _BSD_SOURCE
#include <sys/types.h>
#endif
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define PROTOCOL_NAME "UDP"

struct recv_args {
	void* (*callback) (void*);
	int socket;
	void *upperLayerArgs;
	struct threadManager *manager;
};

inline int getProtocolNumber(void);
inline void logger(char *message);
int openSocket(int *fd, const char *localAddr, const char *localPort, const char *remoteAddr, const char *remotePort);
int openSender(int *fd, const char *remoteAddr, const char *remotePort);
p2pError createCallback(void *(*callback)(void *), const int socket, void *callbackArgs, struct p2pChannel *p2p);
void receiver(struct recv_args *args);
p2pError openReceiver(int *fd, const char* localPort);

/*
 * Initialize a P2P channel, i.e. create a p2p-receiver.
 * callback is the function to call when receiving a message on the resulting p2pChannel.
 * localPort is our local port to open.
 */

p2pError init_p2p(const char *localPort, const char *localAddr, struct p2pChannel *p2p, void *(*callback)(void*), void *callbackArgs)
{
	int *fd, recv;
	int res;
	int initSize = 1;
	memset(p2p, 0, sizeof(struct p2pChannel));
	initManager(&p2p->manager);
	res = openReceiver(&recv, localPort);
	if(res != SUCCESS)
	{
		fprintf(stderr, "Error creating receiver\n");
		return res;
	}
	p2p->receiver = recv;

	fd = malloc(initSize * sizeof(int));
	if(fd == NULL)
	{
		fprintf(stderr, "Allocation error\n");
		return EALLOC;
	}
	p2p->senders = fd;
	p2p->sendersSize = initSize; 
	
	p2p->nb_senders = 0;	
	p2p->removed_senders = 0;

	res = createCallback(callback, p2p->receiver, callbackArgs, p2p);
	if(res != SUCCESS)
	{
		fprintf(stderr, "Cannot create pthread");
		return res;
	}
	return SUCCESS;
}

/*
 * Delete the P2P channel. First ensure that all senders are deallocated.
*/
p2pError kill_p2p(struct p2pChannel *p2p)
{
	if(p2p->nb_senders > p2p->removed_senders)
		return ESENDER_EXIST;
	free(p2p->senders);
	return SUCCESS;
}

/*
 * Add a sender to the set of senders of the given p2p channel.
 * Undefined behavior if p2p has not been successfully initialized by init_p2p.
 * Not thread-safe
*/
p2pError init_p2p_sender(const char *remotePort, const char *remoteAddr, 
		struct p2pChannel *p2p)
{
	int s_send;
	if(openSender(&s_send, remoteAddr, remotePort) != SUCCESS)
	{
		fprintf(stderr, "openSocket() failed\n");
		return EINIT;
	} 
	
	if(p2p->nb_senders == p2p->sendersSize)
	{
		size_t newSize = 2*p2p->sendersSize;
		int* newSendersArray = (int*) realloc(p2p->senders,
				newSize*sizeof(int));
		if(newSendersArray == NULL)
		{
			fprintf(stderr, "realloc() failed\n");
			close(s_send);
			return EALLOC;
		}	
		p2p->senders = newSendersArray;
		p2p->sendersSize = newSize;
	}
	p2p->senders[p2p->nb_senders] = s_send;
	p2p->nb_senders++;

#ifdef LOGGER
	fprintf(stderr, ANSI_YELLOW "[p2p]\tInitialized p2p sender to %s:%s\n" ANSI_RESET, remoteAddr, remotePort);
#endif
	return SUCCESS;
}

p2pError delete_p2p_sender(const size_t index, struct p2pChannel *p2p)
{
	int sender = p2p->senders[index];
	if(close(sender) != 0)
	{
		fprintf(stderr, "Error closing socket : %s", strerror(errno));
		return ECLOSE;
	}
	p2p->senders[index] = 0;
	p2p->removed_senders++;
	return SUCCESS;
}

int openSender(int *fd, const char *remoteAddr, const char *remotePort)
{
	int protocol = getProtocolNumber();
	struct addrinfo hints;
	struct addrinfo *results, *rp;
	int s, sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = protocol;

	s = getaddrinfo(remoteAddr, remotePort, &hints, &results);
	
	if(s != 0)
	{
		fprintf(stderr, "getaddrinfo : %s\n", gai_strerror(s));
		return ECREATE;
	}
	for (rp = results; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

#pragma GCC diagnostic ignored "-Wmaybe-uninitialized" /* uninialized checked above */
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;                  /* Success */

		close(sfd);
	}
	freeaddrinfo(results);
	*fd = sfd;
	return SUCCESS;
}

p2pError p2pSend(const struct p2pChannel *chan, const int index, const void* buf, const size_t length)
{
	if(chan->senders[index] == 0)
		return ESENDER_DELETED;
#ifdef LOGGER
	fprintf(stderr, ANSI_YELLOW "[p2p]\tSending \"");
	fwrite(buf, length, 1, stderr);
	fprintf(stderr, "\" to %i\n" ANSI_RESET, index);
#endif
	if (index >= chan->nb_senders)
	{
		fprintf(stderr, "Failure : trying to send to %i-th sender while %zi exists", index, chan->nb_senders);
		return ESEND;
	}
	if (send(chan->senders[index], buf, length, MSG_DONTWAIT) == -1)
	{
		fprintf(stderr, "Error sending message : %s\n", strerror(errno));
		switch(errno){
			case EBADF: 
				fprintf(stderr, "File descriptor : %i\n", chan->senders[index]);
				break;
			default: 
				break;
		}	
		return ESEND;
	}
	return SUCCESS;
}

p2pError createCallback(void *(*callback)(void *), const int socket, void *callbackArgs, struct p2pChannel *p2p)
{
	pthread_t thread;
	pthread_attr_t attr;
	struct recv_args *args;
	if(pthread_attr_init(&attr) != 0)
	{
		fprintf(stderr, "pthread_attr_init() failed\n");
		return 	EPTHREAD;
	}
	args = malloc(sizeof(struct recv_args));
	args -> callback = callback;
	args -> socket = socket;
	args -> upperLayerArgs = callbackArgs;
	args -> manager = &p2p->manager;
	int res = requestThread(&thread, &attr, (void*(*)(void*))receiver, args, &p2p->manager);
	if (res != 0) 
	{
		fprintf(stderr, "requestThread() failed\n");
	}
	return res;
}

void receiver(struct recv_args *args)
{
	void* (*callback)(void*) = args->callback;
	int socket = args->socket;
	void *upperLayerArgs = args->upperLayerArgs;
	struct threadManager* manager = args->manager;
	free(args);
	struct p2pMessage *buf;
	for(;;)
	{
#ifdef LOGGER
		fprintf(stderr,  ANSI_YELLOW "[p2p]\tWaiting for a message\n" ANSI_RESET);
#endif
		pthread_t thread;
		pthread_attr_t attr;
		buf = malloc(sizeof(struct p2pMessage));
		if(pthread_attr_init(&attr) != 0)
		{
			fprintf(stderr, "pthread_attr_init() failed\n");
		}
		recv(socket, &buf->message, sizeof(struct p2pMessage), 0);
		buf->upperLayerArgs = upperLayerArgs;
		int res = requestThread(&thread, &attr, callback, buf, manager);
		if(res != 0)
		{
			fprintf(stderr, "requestThread() failed : %s\n", strerror(res));
		}
	}
}


p2pError openReceiver(int *fd, const char* localPort)
{
#ifdef LOGGER
	fprintf(stderr, ANSI_YELLOW "[p2p]\tOpening receiver on port %s\n" ANSI_RESET, localPort);
#endif
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(NULL, localPort, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;                  /* Success */

		close(sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);           /* No longer needed */
	*fd =sfd;
	return SUCCESS;
}

inline int getProtocolNumber(void)
{
	struct protoent *protocol = getprotobyname(PROTOCOL_NAME);
	return protocol->p_proto;
}

