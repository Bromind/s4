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

#define PROTOCOL_NAME "UDP"

struct recv_args {
	void* (*callback) (void*);
	int socket;
};

inline int getProtocolNumber(void);
inline void logger(char *message);
int openSocket(int *fd, const char *localAddr, const char *localPort, const char *remoteAddr, const char *remotePort);
int openSender(int *fd, const char *remoteAddr, const char *remotePort);
void createCallback(void *(*callback)(void *), const int socket);
void receiver(struct recv_args *args);

/*
 * Initialize a P2P channel, i.e. create a p2p-receiver.
 * callback is the function to call when receiving a message on the resulting p2pChannel.
 * localPort is our local port to open.
 */

p2pError init_p2p(const char *localPort, const char *localAddr, struct p2pChannel *p2p, void *(*callback)(void*))
{
	int *fd, recv;
	int res;
	int initSize = 1;
	memset(p2p, 0, sizeof(struct p2pChannel));
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
	createCallback(callback, p2p->receiver);
	return SUCCESS;
}

/*
 * Add a sender to the set of senders of the given p2p channel.
 * Undefined behavior if p2p has not been successfully initialized by init_p2p.
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
#ifdef LOGGER
	fprintf(stderr, ANSI_YELLOW "[p2p]\tSending \"%s\"\n" ANSI_RESET, buf);
#endif
	if (index >= chan->nb_senders)
	{
		fprintf(stderr, "Failure : trying to send to %i-th sender while %i exists", index, chan->nb_senders);
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

void createCallback(void *(*callback)(void *), const int socket)
{
	pthread_t thread;
	pthread_attr_t attr;
	struct recv_args *args;
	if(pthread_attr_init(&attr) != 0)
	{
		fprintf(stderr, "pthread_attr_init() failed\n");
	}
	args = malloc(sizeof(struct recv_args));
	args -> callback = callback;
	args -> socket = socket;
	pthread_create(&thread, &attr, (void*(*)(void*))receiver, args);
}

void receiver(struct recv_args *args)
{
	void* (*callback)(void*) = args->callback;
	int socket = args->socket;
	struct p2pMessage buf;
	for(;;)
	{
#ifdef LOGGER
		fprintf(stderr,  ANSI_YELLOW "[p2p]\tWaiting for a message\n" ANSI_RESET);
#endif
		recv(socket, &buf.message, sizeof(struct p2pMessage), 0);
		callback(&buf);
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
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	ssize_t nread;

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

