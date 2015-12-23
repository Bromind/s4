#include <stddef.h>
#include "error.h"

#define P2P_BUFF_SIZE 50

typedef int p2pError ;

struct p2pChannel {
	int receiver;
	int *senders;
	size_t nb_senders;
	size_t sendersSize;
	size_t removed_senders;
};

struct p2pMessage {
	char message[P2P_BUFF_SIZE];
	void *upperLayerArgs;
};

struct p2pCallBack;

p2pError init_p2p(const char *localPort, const char *localAddr, 
		struct p2pChannel *p2p, void *(*callback)(void*),
		void *callbackArgs);

p2pError init_p2p_sender(const char *remotePort, const char *remoteAddr,
		struct p2pChannel *p2p);

p2pError p2pSend(const struct p2pChannel *chan, const int index, const void* buf, const size_t length);

p2pError kill_p2p(struct p2pChannel *p2p);

p2pError delete_p2p_sender(const size_t index, struct p2pChannel *p2p);
