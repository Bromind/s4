#include <stddef.h>

#define EFAIL -1
#define SUCCESS 0
#define ECREATE 1
#define EINET 2
#define EBIND 3
#define EINIT 4
#define ESEND 5
#define EALLOC 6

#define BUFF_SIZE 25

typedef int p2pError ;

struct p2pChannel {
	int receiver;
	int *senders;
	size_t nb_senders;
	size_t sendersSize;
};

struct p2pMessage {
	char message[BUFF_SIZE];
};

struct p2pCallBack;

p2pError init_p2p(const char *localPort, const char *localAddr, 
		struct p2pChannel *p2p, void *(*callback)(void*));

p2pError init_p2p_sender(const char *remotePort, const char *remoteAddr,
		struct p2pChannel *p2p);

p2pError p2pSend(const struct p2pChannel *chan, const int index, const void* buf, const size_t length);
